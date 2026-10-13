#pragma once

#include "frame_mailbox.h"
#include "remote_displays.h"
#include "view_quality.h"
#include "viewer_audio.h"
#include "viewer_chat.h"
#include "viewer_clipboard.h"
#include "viewer_files.h"
#include "viewer_power.h"

#include <rc/data_channels.h>
#include <rc/transport.h>

#include <rcsdk/rcsdk.h>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class ServiceClient;

namespace rcsdk {
class Session;
class SessionConfig;
}

// 去连一台设备（观看端）：SDK 的观看会话，信令经后台服务和服务器交给对方（rc/protocol.h 的 signal）。
// 先直连（P2P，带服务器给的 STUN）；打不通、或者对方正经中继服务别人时，有中继（服务器给的 TURN，ice 主题）
// 就经中继再连一次（rc/transport.h）。
// 连别人要对方的临时密码（SDK 的 PIN，只在两台机器之间验证）。连我的设备（账号下别的设备）不要密码：
// 一打开就连，只靠两边的设备密钥（rc/transport.h 的 DeviceAccess）；对方不认这台设备时（没登录同一个账号、
// 还没从服务器知道本机）再要临时密码。
// 解出的画面放进 frames()，由会话窗口里的 RemoteView 画；对方的指针单独传来（cursor），本机的输入经 sendInput 发过去。
// 连上以后两边的消息走 SDK 的数据通道，按通道号（rc/peer_protocol.h）交给各自的处理者：对话是 chat，
// 对方的显示器是 displays。
// 和对方互传的文件在 files（重新连接时还是这一份）。
// 我的设备还能让它重启、关机、睡眠、注销（ViewerPower，对方提供才有），对方答应以后连接断了：重启、注销的
// 等它回来（账号里它又在线了）马上再连（status 是 restarting、signingOut），关机、睡眠的停下（shutDown、asleep）。
//
// status：needPin   等用户输入对方的临时密码（reason 说明上一次为什么不行）
//         connecting 正在连 | waiting 密码对了，等对方的人点允许
//         connected 已连上 | reconnecting 路断了，正在重连
//         failed    连不上、断了（reason 说明原因）
//         restarting、signingOut 对方在重启、注销，过一会儿自己再连 | shutDown、asleep 对方关机、睡眠了
class ViewerSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由 ViewerSessions 创建")

    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString reason READ reason NOTIFY stateChanged)
    Q_PROPERTY(bool controlGranted READ controlGranted NOTIFY controlChanged)
    // 分辨率、帧率（view_quality.h）：选的长边（0：原始分辨率）、每秒几帧（0：对方显示器的刷新率），运行中随时换
    Q_PROPERTY(int resolutionChoice READ resolutionChoice WRITE setResolutionChoice NOTIFY qualityChanged)
    Q_PROPERTY(int frameRateChoice READ frameRateChoice WRITE setFrameRateChoice NOTIFY qualityChanged)
    // 对方正在分享的那块显示器能选的：[{label, value}]（value 是上面的选择），选的落在第几个。
    // 还不知道对方的显示器时分辨率为空、-1
    Q_PROPERTY(QVariantList resolutionOptions READ resolutionOptions NOTIFY qualityChanged)
    Q_PROPERTY(int resolutionIndex READ resolutionIndex NOTIFY qualityChanged)
    Q_PROPERTY(QVariantList frameRateOptions READ frameRateOptions NOTIFY qualityChanged)
    Q_PROPERTY(int frameRateIndex READ frameRateIndex NOTIFY qualityChanged)
    // 码率（Mbit/s，0：自动）和编码格式（rcs_codec，0：自动）；编码格式的选项连上、对方说了用显卡编得了哪几种以后才有
    Q_PROPERTY(int bitrateChoice READ bitrateChoice WRITE setBitrateChoice NOTIFY qualityChanged)
    Q_PROPERTY(QVariantList bitrateOptions READ bitrateOptions NOTIFY qualityChanged)
    Q_PROPERTY(int bitrateIndex READ bitrateIndex NOTIFY qualityChanged)
    Q_PROPERTY(int codecChoice READ codecChoice WRITE setCodecChoice NOTIFY qualityChanged)
    Q_PROPERTY(QVariantList codecOptions READ codecOptions NOTIFY qualityChanged)
    Q_PROPERTY(int codecIndex READ codecIndex NOTIFY qualityChanged)
    // 文字更清楚：要 4:4:4 的画面（颜色精细到每个像素，字边不糊，码率高一些）。两边都用显卡做得了 4:4:4 的
    // 编码格式才行（clearTextAvailable）；打开时编码器只列这几种
    Q_PROPERTY(bool clearText READ clearText WRITE setClearText NOTIFY qualityChanged)
    Q_PROPERTY(bool clearTextAvailable READ clearTextAvailable NOTIFY qualityChanged)
    // 低延迟模式：两边都用 SDK 的低延迟预设（对方输入后加速采集、发得更快，本机的解码、声音缓冲更短），
    // 适合局域网里操作。对方听主观看端的
    Q_PROPERTY(bool lowLatency READ lowLatency WRITE setLowLatency NOTIFY qualityChanged)
    // 画面听不听本机的：对方同时被几个人连着时，画质、尺寸按先连上的那个人要的出，后连上的改了不起作用
    Q_PROPERTY(bool mainViewer READ mainViewer NOTIFY optionsChanged)
    Q_PROPERTY(RemoteDisplays *displays READ displays CONSTANT)
    Q_PROPERTY(int latencyMs READ latencyMs NOTIFY statsChanged)
    // 直连、局域网直连、中继；还不知道时为空
    Q_PROPERTY(QString connectionType READ connectionType NOTIFY statsChanged)
    // 连接上每秒收发多少（画面为主，也有声音和消息）
    Q_PROPERTY(double bitrateMbps READ bitrateMbps NOTIFY statsChanged)
    Q_PROPERTY(int framesPerSecond READ framesPerSecond NOTIFY statsChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY statsChanged)
    Q_PROPERTY(QString codec READ codec NOTIFY statsChanged)
    // 远程声音、剪贴板同步，会话里随时开关（ViewerAudio、ViewerClipboard）；开着却听不到、同步不了时的一句说明
    Q_PROPERTY(bool audio READ audio WRITE setAudio NOTIFY optionsChanged)
    Q_PROPERTY(bool clipboard READ clipboard WRITE setClipboard NOTIFY optionsChanged)
    Q_PROPERTY(QString audioNote READ audioNote NOTIFY optionsChanged)
    Q_PROPERTY(QString clipboardNote READ clipboardNote NOTIFY optionsChanged)
    Q_PROPERTY(ViewerChat *chat READ chat CONSTANT)
    Q_PROPERTY(ViewerFiles *files READ files CONSTANT)
    // 对方提供的电源操作、要了以后的进展（ViewerPower）
    Q_PROPERTY(QStringList powerActions READ powerActions NOTIFY powerChanged)
    Q_PROPERTY(QVariantMap powerRequest READ powerRequest NOTIFY powerChanged)
    // 现在叫不叫得醒它（DeviceWake::problem）：叫得醒时为空，不然是为什么
    Q_PROPERTY(QString wakeProblem READ wakeProblem NOTIFY wakeChanged)

public:
    // 对方的指针：热点在共享的显示器上的位置（0..65535），形状的编号
    struct Cursor
    {
        bool visible = false;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t shapeId = 0;
    };
    // 指针的图像，按对方显示器的像素
    struct CursorShape
    {
        QImage image;
        QPoint hotspot;
    };

    // target：要连的设备 ID（空格、"-"不算），或者同一网络里的 IP 地址；都不是时一开始就是 failed。
    // decodeDevice：Windows 上会话窗口画画面用的 D3D11 设备（ViewGraphics），SDK 在它上面解码；
    // Windows 上为空时画面是 CPU 上的图像。conversation：和这台设备的对话；transfers：没传完的文件记在哪里；
    // mineKey：设备 ID 是我的设备时它的公钥（Account::publicKey），不是时为空
    ViewerSession(const QString &target, ServiceClient &service, void *decodeDevice, Conversation &conversation,
                  rc::InterruptedTransfers &transfers, std::function<QByteArray(const QString &deviceId)> mineKey,
                  QObject *parent = nullptr);
    ~ViewerSession() override;

    QString status() const { return status_; }
    QString reason() const { return reason_; }
    bool controlGranted() const { return controlGranted_; }
    int resolutionChoice() const { return int(resolutionChoice_); }
    void setResolutionChoice(int longSide);
    int frameRateChoice() const { return int(frameRateChoice_); }
    void setFrameRateChoice(int frameRate);
    QVariantList resolutionOptions() const;
    int resolutionIndex() const;
    QVariantList frameRateOptions() const;
    int frameRateIndex() const;
    int bitrateChoice() const { return int(bitrateChoice_); }
    void setBitrateChoice(int mbps);
    QVariantList bitrateOptions() const;
    int bitrateIndex() const;
    int codecChoice() const { return int(codecChoice_); }
    void setCodecChoice(int codec);
    QVariantList codecOptions() const;
    int codecIndex() const;
    bool clearText() const { return clearText_; }
    void setClearText(bool clearText);
    bool clearTextAvailable() const { return !view_quality::clearTextCodecs(codecs()).isEmpty(); }
    bool lowLatency() const { return lowLatency_; }
    void setLowLatency(bool lowLatency);
    bool mainViewer() const { return mainViewer_; }
    RemoteDisplays *displays() const { return displays_; }
    int latencyMs() const { return latencyMs_; }
    QString connectionType() const { return connectionType_; }
    double bitrateMbps() const { return bitrateMbps_; }
    int framesPerSecond() const { return framesPerSecond_; }
    QString resolution() const { return resolution_; }
    QString codec() const { return codec_; }
    bool audio() const { return audio_.enabled(); }
    void setAudio(bool audio);
    bool clipboard() const { return clipboard_.enabled(); }
    void setClipboard(bool clipboard);
    QString audioNote() const { return audio_.note(); }
    QString clipboardNote() const { return clipboard_.note(); }
    ViewerChat *chat() { return &chat_; }
    ViewerFiles *files() const { return files_; }
    QStringList powerActions() const { return power_->offered(); }
    QVariantMap powerRequest() const { return power_->request(); }
    QString wakeProblem() const;

    // 让对方电脑 "restart" | "shutDown" | "sleep" | "signOut"（界面上确认过了）
    Q_INVOKABLE void requestPower(const QString &action);
    // 对方睡眠、关机了：从本机发唤醒包，等它上线就连（status 是 waking，最多等 3 分钟）
    Q_INVOKABLE void wake();
    // 用户输入的临时密码：这次按临时密码连
    Q_INVOKABLE void connectWithPin(const QString &pin);
    // 用上一次的密码（我的设备：设备密钥）再连一次
    Q_INVOKABLE void retry();
    void stop();

    // 这一次连接的编号（信令里的 session）；没在连时是空的
    QString connection() const { return connection_; }
    // 对方的回答
    void receiveSignal(const QString &text);
    // 信令没送到；reason 见 rc/protocol.h 的 Undeliverable
    void undeliverable(const QString &reason);
    // 对方现在不接受连接，回绝了；reason 见 rc/peer_protocol.h
    void declined(const QString &reason);

    std::shared_ptr<FrameMailbox> frames() const { return frames_; }
    // 正在收的画面：尺寸、画面在编码帧里的位置、对方的显示器；没在连时全是 0
    rcs_stream_info streamInfo() const;
    // 对方的系统；还没连上时是 RCS_PLATFORM_UNKNOWN
    rcs_platform peerPlatform() const;
    // 对方执行哪些输入（rcs_peer_info.input_types）；还没连上时是 0
    uint32_t peerInputTypes() const;
    Cursor cursor() const { return cursor_; }
    // 这次连接里见过的形状；没见过的编号为空
    const CursorShape *cursorShape(uint32_t id) const;
    // 对方允许控制（controlGranted）时才会被执行
    void sendInput(const rcs_input_event &event);
    // 数据通道：发给对方一条（连上以后）
    rc::peer::SendResult sendData(std::uint32_t channel, const QByteArray &data);

signals:
    void stateChanged();
    void controlChanged();
    void statsChanged();
    void optionsChanged();
    // 选的分辨率、帧率，或者对方的显示器（能选的）变了
    void qualityChanged();
    // 画面要在界面线程上更新：第一帧、形状变了的帧、CPU 上的每一帧、断开以后
    void frameReady();
    // 画面的尺寸、在编码帧里的位置、对方的显示器变了
    void streamChanged();
    void cursorChanged();
    void powerChanged();
    void wakeChanged();

private:
    // relay：经中继
    void open(bool relay);
    void endSignal();
    // 按我的设备连：对方是我的设备，本机的设备密钥也有，对方还没说过不认
    bool byDevice() const;
    // 对方不认这台设备：改用临时密码，reason 告诉用户为什么
    void usePassword(const QString &reason);
    rc::IceServers iceServers() const;
    void useTransport(rcsdk::SessionConfig &config, const rc::IceServers &ice) const;
    // 服务器换了凭据：连着的从下一次重连起用新的
    void updateIceServers();
    void stateEvent(rcs_state state);
    void admissionEvent(rcs_admission_state state, rcs_result result, const QString &message);
    void setStatus(const QString &status, const QString &reason = {});
    // 正在连的时候显示什么：对方在重启、注销时还是 restarting、signingOut
    QString connectingStatus() const;
    // 对方答应的电源操作开始了，连接断了
    void afterPower();
    // 连接出了问题（失败、被回绝、送不到、对方没回应）：对方刚答应了电源操作（这时 SDK 可能还在自己换路重连，
    // 信令到了刚起来的对方那里被回绝），或者正等它重启、注销回来，由这里接着办；都不是时返回 false
    bool handledByPower();
    void waitForPeer();
    // 账号里对方现在在不在线（只有我的设备才知道）
    bool peerOnline() const;
    void refreshStats();
    // 控制通道（显示器）发给对方
    void sendControl(const QByteArray &message);
    // 按画质向对方要画面；对方换了显示器时原始分辨率跟着变
    void requestQuality();
    // 两边都能用的编码格式（view_quality::codecs）；还不知道对方用显卡编得了哪几种时为空
    QList<view_quality::Codec> codecs() const;
    // 编码器的选项：要文字更清楚、也做得了时只有做得了 4:4:4 的
    QList<view_quality::Codec> codecChoices() const;
    // 告诉对方用不用低延迟预设（连上时、改了时）
    void tellLowLatency();
    // 对方说画面听不听本机的（rc/control_protocol.h 的 mainViewer）；连接结束时回到听
    void setMainViewer(bool main);
    // SDK 的线程：通知界面线程画面要更新，没处理的只排一个
    void notifyFrame();

    ServiceClient &service_;
    void *decodeDevice_;
    // 要连的：设备 ID（9 位数字），或者局域网里的 IP 地址（按地址连），只有一个不空
    QString deviceId_;
    QString address_;
    QString target_; // 日志里用
    std::function<QByteArray(const QString &)> mineKey_;
    QString pin_;
    bool byPassword_ = false; // 用户输入了临时密码，或者对方不认这台设备
    QString access_{rc::PasswordAccess}; // 这一次连接怎么证明自己（rc/transport.h）
    QString connection_;
    bool relay_ = false; // 这一次连接经中继
    QString status_ = QStringLiteral("needPin");
    QString reason_;
    bool controlGranted_ = false;
    uint32_t resolutionChoice_ = 1920;
    uint32_t frameRateChoice_ = 60;
    uint32_t bitrateChoice_ = 0;
    rcs_codec codecChoice_ = 0;
    bool clearText_ = false;
    bool lowLatency_ = false;
    // 本机用显卡、CPU 解得了的编码格式；对方用显卡编得了的（它连上以后告诉本机，rc/control_protocol.h）
    uint32_t hardwareDecoders_ = 0;
    uint32_t softwareDecoders_ = 0;
    // 本机用显卡解得了 4:4:4 的编码格式
    uint32_t clearTextDecoders_ = 0;
    std::optional<uint32_t> peerHardware_;
    bool mainViewer_ = true;
    // 现在向对方要的画面，和对方正在分享的显示器（还不知道时宽为 0）
    rcs_view_request_config request_ = rcs_view_request_config_init();
    view_quality::Display display_;
    int latencyMs_ = 0;
    QString connectionType_;
    double bitrateMbps_ = 0;
    quint64 lastBytes_ = 0;
    int framesPerSecond_ = 0;
    QString resolution_;
    QString codec_;
    ViewerAudio audio_;
    ViewerClipboard clipboard_;
    quint64 lastFrames_ = 0;
    QTimer statsTimer_;
    QTimer answerTimer_; // 对方一直不回答就别一直等
    std::shared_ptr<FrameMailbox> frames_ = std::make_shared<FrameMailbox>();
    std::atomic<bool> frameQueued_ = false;
    Cursor cursor_;
    QHash<uint32_t, CursorShape> cursorShapes_;
    rc::peer::DataChannels channels_;
    ViewerChat chat_;
    RemoteDisplays *displays_;
    ViewerFiles *files_;
    ViewerPower *power_;
    // 对方在重启、注销：等它回来，到 until 为止。它在线时（还没下线，或者刚回来连不上）隔一会儿再连，
    // 不在线时等账号里它再上线
    struct PowerWait
    {
        QString action;
        QDeadlineTimer until;
    } powerWait_;
    bool peerWasOnline_ = false;
    QTimer retryTimer_;
    // 放在最后：先析构它（停下会话、等回调都结束），再析构别的
    std::unique_ptr<rcsdk::Session> session_;
};
