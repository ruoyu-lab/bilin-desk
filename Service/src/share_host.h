#pragma once

#include "share_clipboard.h"
#include "share_displays.h"
#include "share_thumbnails.h"
#include "share_transport.h"

#include <rc/data_channels.h>
#include <rc/file_transfers.h>
#include <rc/protocol.h>
#include <rc/transport.h>

#include <rcsdk/rcsdk.h>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QHash>
#include <QSet>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>

class DeviceStore;

namespace rcsdk {
class AgentGate;
class Session;
class SessionConfig;
}

// 采集、声音、键鼠在哪里做（docs/install-and-first-run.md）
struct ShareCapture
{
    enum class Where {
        // 开发版：后台服务自己的进程里
        Here,
        // Windows 装好时：也在自己的进程里（以 SYSTEM 在控制台会话里运行），跟着锁屏、登录界面、UAC 这些安全桌面，
        // 能发 Ctrl+Alt+Del
        SecureDesktops,
        // macOS 装好时：各个会话里的助手（SDK 的 agent link，rc/system_service.h），授权是助手的
        Agents,
    };
    Where where = Where::Here;
    QByteArray agentKey; // Agents：链接的密钥（system_mode::agentKey）
};

// 被控的一面：SDK 的分享会话。别的设备经服务器的信令连进来（P2P：局域网和跨网络是同一种连法；
// 直连打不通的走中继，一次运行只能是一种，什么时候换见 ShareTransport）。
// 一次分享只认一种身份（SDK），所以有两个（Sharing 把它们合在一起），access 说是哪一个（rc/transport.h）：
// - 别人（PasswordAccess）：临时密码就是 SDK 的 PIN（只在两台机器之间验证，不经过服务器），本机的人点允许。
//   只在"允许别人连接本机"开着时运行；临时密码换了，从下一个握手起用新的
// - 我的设备（DeviceAccess）：只靠设备密钥（RCS_SECRET_NONE），信任的是账号下别的设备的公钥（setTrustedKeys）。
//   SDK 核对过就直接让进、能控制，不要密码、不用点允许，没有界面连着也一样；不管"允许别人连接本机"开没开。
//   账号下没有别的设备时不运行；刚启动、服务器还没告诉本机账号时也不运行，来连的回绝说还没准备好（不是说不认它）
// 下面说的临时密码、请求、点允许都只是"别人"的：
// - macOS 要授权：没有"屏幕与系统音频录制"别人连不上，没有"辅助功能"只能让对方看。系统不通知授权变了，
//   缺授权时定时去看，授了就开始（没人连着时按新的授权重开分享会话）
// - 不接受连接时（关着、缺授权、出错了），来连的人经服务器收到原因（declined）
// - 密码对了还要本机的人点允许（界面上的连接请求，state() 的 requests），可以只让对方看、不让控制；
//   没有界面连着后台服务时没人能点，直接拒绝。一分钟没人点也拒绝（SDK 的默认）
// - 临时密码用过就换：请求没被允许（拒绝、没人点、对方走了），或者连进来的人走了。
//   连着的人只是路断了（网络）时等它一分钟内回来，回来直接连上、控制权照旧，过了才换
// - 试错次数用完，也换一个新的临时密码
// - 声音：本机正在播放的声音发给连着的人，不收对方的。剪贴板：见 ShareClipboard，本机的人可以关掉
// 连着的人之间经 SDK 的数据通道收发消息，按通道（rc/peer_protocol.h）交给各自的处理者：
// 对话在 ShareChat（经 addChannel 接上），多显示器（换着分享哪一块）在 ShareDisplays
// - 和连着的人互传文件（files()）：对方发来的由界面上的人决定收不收，没有界面连着时直接拒绝
// - 我的设备让本机重启、关机、睡眠、注销（SDK 的电源模块）：只给我的设备，别人的电脑一律不能
class ShareHost : public QObject
{
    Q_OBJECT

public:
    // access：rc/transport.h 的 PasswordAccess（别人）| DeviceAccess（我的设备）；
    // transfers：和连着本机的人没传完的文件记在哪里（rc::FileTransfers）
    ShareHost(QLatin1StringView access, const ShareCapture &capture, DeviceStore &store,
              rc::InterruptedTransfers &transfers, QObject *parent = nullptr);
    ~ShareHost() override;

    // 我的设备：账号下别的设备的公钥（65 字节），从下一个握手起只认它们；空的、还不知道（nullopt）：不运行
    void setTrustedKeys(const std::optional<QList<QByteArray>> &keys);

    // 有没有界面连着后台服务（有人能点允许）
    void setAttended(bool attended);
    rc::protocol::ShareState state() const;
    // 和连着本机的人互传的文件；分享会话停了就清空
    rc::FileTransfers &files() { return files_; }
    const rc::FileTransfers &files() const { return files_; }
    // 弹出系统的授权提示（只有第一次会弹，之后要在系统设置里打开）。permission：rc/protocol.h 的
    // ScreenPermission | InputPermission。要在主线程调用
    void requestPermission(const QString &permission);
    // 出了错（problem 是 unavailable）以后再试一次
    void restart();
    // P2P 和中继用的服务器（服务器发来的）：从下一个连接起用
    void setIceServers(const rc::IceServers &ice);

    // 设备 from（名字 name）发来的信令：它要这样（mode）连本机，mode.access 和这一个的一样
    void receiveSignal(const QString &from, const QString &name, const QString &session, const QString &text,
                       const rc::ConnectionMode &mode);
    // 界面上的人对连接请求、连着的人的决定（id 见 state()）
    void admit(int id, bool control);
    void refuse(int id);
    void disconnectViewer(int id);
    void setControl(int id, bool control);
    // 断开所有连着本机的人（界面程序要退出了、没了），路断了在回来的也不等了
    void disconnectAll();
    // 后台服务要被系统重启（装好时：注销、换用户、服务停了）：停下，连着的人知道这边会回来，自己重连
    void stopForRestart();
    // 和连着本机的人同步不同步剪贴板（所有人一起）
    void setClipboard(bool clipboard);

    // 数据通道：连着本机的人发来的按通道号交给 handler（在这个对象的线程上），发给连着本机的人 id
    void addChannel(std::uint32_t channel, rc::peer::DataChannels::Handler handler);
    rc::peer::SendResult sendData(int id, std::uint32_t channel, const QByteArray &data);

signals:
    // 给设备 to 的回答
    void answer(const QString &to, const QString &session, const QString &text);
    // 本机现在不接受连接，回绝设备 to 的这次连接（reason 见 rc/peer_protocol.h）
    void declined(const QString &to, const QString &session, const QString &reason);
    void stateChanged();
    // 连接记录（ConnectionHistory）用：连进来了、走了（session 认出同一次连接：路断了又回来的还是那一次），
    // 请求没成（reason：refused | timeout | unattended，见 rc::protocol::HistoryEntry），有人输错了临时密码
    void viewerConnected(const rc::protocol::SharePeer &viewer, const QString &session);
    void viewerGone(const QString &device, const QString &session);
    void requestFailed(const rc::protocol::SharePeer &request, const QString &reason);
    void wrongPin();
    // 本机答应了连着的人 viewer，马上就 action（restart | shutDown | sleep | signOut）
    void powerDone(const rc::protocol::SharePeer &viewer, const QString &action);

private:
    // SDK 认的路由号 ↔ 哪台设备的哪次连接（宿主自己定，SDK 原样带回）
    struct Route
    {
        QString device;
        QString name;
        QString session;
        QDeadlineTimer expires;
    };
    // 正连着、或者在等本机点允许的一个人
    struct Peer
    {
        rc::protocol::SharePeer info;
        QString session;
        // 给它录不了声音时为什么（rc/control_protocol.h 的 SoundNoDevice …），已经告诉过它；录得了时是空的
        QLatin1StringView sound;
        // 它要低延迟模式（rc/control_protocol.h 的 lowLatency）
        bool lowLatency = false;
    };
    // 路断了的人：一分钟内同一次连接回来就直接让它进
    struct Returning
    {
        QString device;
        QString session;
        bool control = false;
        QDeadlineTimer expires;
    };

    bool mine() const { return access_ == rc::DeviceAccess; }
    // 路由的那次连接还连着、在等允许，或者路断了在等它回来
    bool inUse(const Route &route) const;
    // 该不该运行：别人的开着"允许别人连接本机"，我的设备的账号下有别的设备（还要界面程序开着，或者允许没人开界面时也连）
    bool wanted() const;
    void update();
    void start();
    // 分享会话怎么认人：临时密码，或者设备密钥和信任的公钥
    void useIdentity(rcsdk::SessionConfig &config) const;
    void stop();
    // 停下分享会话，用 transport 重新开
    void reopen(const QString &transport);
    // 分享会话用的传输方式和它的服务器（STUN、TURN）
    void useTransport(rcsdk::SessionConfig &config) const;
    // 有人连着、在等允许、路断了在回来的路上
    bool inUse() const;
    // 用着中继、人都走了：换回直连
    void switchBackIfIdle();
    // 停下分享会话，连着的、等着的都不要了
    void end();
    void runFailed();
    void readPermissions();
    void checkPermissions();
    // 授权变了以后：没开就开，有人连着时不动，能不能控制变了就重开
    void permissionsChanged(bool changed);
    // 助手连上来、走了（Agents）：授权是它的
    void agentChanged(const rcs_agent_info &agent);
    // 我的设备 id 要的电源操作，SDK 答应了（马上就做）
    void powerAccepted(int id, const QString &action);
    void sdkAnswered(quint64 route, const QString &text);
    void useCapture(rcsdk::SessionConfig &config);
    void pinFailed(int attemptsLeft);
    quint64 routeFor(const QString &from, const QString &name, const QString &session);

    // SDK 的事件，搬到这个对象的线程以后。admitted：已经让它进了（我的设备）
    void viewerArrived(int id, quint64 route, const QString &platform, bool admitted);
    // timedOut：一直没人点（不是对方走了）
    void requestEnded(int id, bool timedOut);
    void viewerJoined(int id, quint64 route, const QString &platform, bool control);
    void viewerLeft(int id, bool pathBroke);
    // 给 id 录声音的情况变了（problem 见 Peer::sound，message 是 SDK 说的原因）：告诉它
    void soundChanged(int id, QLatin1StringView problem, const QString &message);
    void forgetReturning();
    Peer peerFor(int id, quint64 route, const QString &platform) const;
    // 告诉 SDK 让不让一个请求进来；reason 是不让时告诉对方的原因（rc/peer_protocol.h）
    void decide(int id, bool admit, bool control, const char *reason = "");
    void changePassword(const char *why);
    // 控制通道（显示器、画面听谁的、声音）发给连着本机的人 viewer（0：连着的每一个）
    void sendControl(int viewer, const QByteArray &message);
    void tellMainViewer();
    // 观看端经控制通道发来的（显示器以外的交给 ShareDisplays）
    void receivedControl(int viewer, const QByteArray &message);
    // 画面是大家共用的：主观看端（先连上的）要低延迟就用 SDK 的低延迟预设，不然用默认的
    void applyLowLatency();
    // 显示器：读本机现在的交给 ShareDisplays；换成分享 id 那块
    void readDisplays();
    bool showDisplay(quint32 id);
    // 走了的、路断了没回来的都不在了：下一个连进来的人从主显示器看起
    void showMainIfNobodyWatches();

    const QLatin1StringView access_;
    const ShareCapture capture_;
    std::unique_ptr<rcsdk::AgentGate> agentGate_; // Agents：认哪个助手
    DeviceStore &store_;
    std::optional<QList<QByteArray>> trustedKeys_;
    ShareTransport transport_;
    rc::IceServers ice_;
    ShareClipboard clipboard_;
    ShareThumbnails thumbnails_;
    bool lowLatency_ = false; // 分享会话现在用着低延迟预设
    std::unique_ptr<rcsdk::Session> session_;
    quint64 run_ = 0; // 分享会话开了第几次：SDK 排着的事件只给开着的这一次
    QString pin_;
    bool attended_ = false;
    // 开着"允许别人连接本机"却没在接受连接时为什么（rc/peer_protocol.h 的 NeedsPermission、Unavailable）
    QString problem_;
    bool screenPermission_ = false;
    bool inputPermission_ = false;
    bool canControl_ = false; // 分享会话开的时候有没有辅助功能授权
    uint32_t hardwareEncoders_ = 0; // 用显卡编得了的编码格式，(1u << rcs_codec) 的位
    QTimer permissionTimer_;
    // 运行中出错以后重新开分享会话（runFailed）；等多久，接连出错时翻倍
    QTimer restartTimer_;
    int restartDelayMs_;
    QHash<quint64, Route> routes_;
    quint64 nextRoute_ = 1;
    QList<Peer> requests_;
    QList<Peer> viewers_;
    QList<Returning> returning_;
    QSet<int> rejoined_; // 路断了又回来的（viewerArrived 让它进了，viewerJoined 时不算新的一次连接）
    rc::peer::DataChannels channels_;
    ShareDisplays displays_;
    rc::FileTransfers files_;
};
