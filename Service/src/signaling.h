#pragma once

#include <rc/transport.h>

#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

// 信令往哪里送、从哪条路走。到后台服务的信令有两种：
// - 界面里去连别人的会话（观看端）发出去的，和对方的回答：界面起的 session，回答要来自当初发给的那台设备
// - 别人来连本机：交给本机的分享会话，它的回答、回绝从对方来的那条路送回去
// 路有两条：服务器，局域网（同一网络里的电脑直接连，没有外网也行）。去连附近的设备、按 IP 地址连走局域网，
// 其余经服务器。收到的信令按 session 认：是界面发出去过、还没结束的就交给界面，其余都是别人来连本机。
// 每份信令带着这次连接怎么连（mode：传输方式、怎么证明自己，rc/transport.h），原样带过去；分享会话的回答
// 带着对方来时的那样（分享端只回答和它一样的，不一样的回绝）。
// 连上以后哪一端换了网络，SDK 都会再发信令找新的路，所以连着的连接一直记着：界面的到界面说结束，
// 来连本机的到那个人走了（没连上的两分钟没动静就忘掉）
class Signaling : public QObject
{
    Q_OBJECT

public:
    // 来连本机、还没连上的，两分钟内没再来信令就忘掉（观看端重连时会再发，重新记上）
    static constexpr int RememberMs = 2 * 60 * 1000;

    // nearbyAddress：附近这台设备在局域网里的地址（LanDiscovery）；不在附近时为空
    explicit Signaling(std::function<QString(const QString &deviceId)> nearbyAddress, QObject *parent = nullptr);

    // ---- 进来的 ----

    // 界面的会话要发给设备 to；address 不空时（按 IP 地址连，to 可以为空）直接走局域网
    void fromViewer(const QString &to, const QString &address, const QString &session, const QString &text,
                    const rc::ConnectionMode &mode);
    // 服务器转来设备 from（名字 name）的信令
    void fromServer(const QString &from, const QString &name, const QString &session, const QString &text,
                    const rc::ConnectionMode &mode);
    // 局域网里 address 那台电脑（设备 from）的信令
    void fromLan(const QString &address, const QString &from, const QString &name, const QString &session,
                 const QString &text, const rc::ConnectionMode &mode);
    // 设备 from 回绝了界面的会话（经服务器或局域网；reason 见 rc/peer_protocol.h）
    void declinedByPeer(const QString &from, const QString &session, const QString &reason);
    // 经服务器发给设备 to 的没送到；reason 见 rc/protocol.h 的 Undeliverable
    void undeliverable(const QString &to, const QString &session, const QString &reason);
    // 局域网里连不上界面的会话要连的那台电脑
    void lanUnreachable(const QString &session);
    // 本机的分享会话对设备 to 的回答、回绝
    void fromSharer(const QString &to, const QString &session, const QString &text);
    void declineFromSharer(const QString &to, const QString &session, const QString &reason);
    // 界面的这次连接结束了
    void endViewer(const QString &session);
    // 来连本机的这次连接连上了：一直记着，直到 releaseSharer（那个人走了）
    void keepSharer(const QString &session);
    void releaseSharer(const QString &session);

signals:
    // ---- 出去的 ----

    void toServer(const QString &to, const QString &session, const QString &text, const rc::ConnectionMode &mode);
    void declineToServer(const QString &to, const QString &session, const QString &reason);
    void toLan(const QString &address, const QString &session, const QString &text, const rc::ConnectionMode &mode);
    // 回答、回绝经局域网来连本机的那次连接
    void answerToLan(const QString &session, const QString &text, const rc::ConnectionMode &mode);
    void declineToLan(const QString &session, const QString &reason);
    void toViewer(const QString &from, const QString &session, const QString &text);
    void viewerUndeliverable(const QString &to, const QString &session, const QString &reason);
    void viewerDeclined(const QString &from, const QString &session, const QString &reason);
    void toSharer(const QString &from, const QString &name, const QString &session, const QString &text,
                  const rc::ConnectionMode &mode);
    // 不再记着这次连接（局域网里它的那条 TCP 连接可以关了）
    void forgotten(const QString &session);

private:
    // 界面的会话：发给谁、走哪条路（address 空的是服务器）
    struct Outgoing
    {
        QString to;
        QString address;
    };
    // 来连本机的：从哪条路来的，怎么连
    struct Incoming
    {
        bool lan = false;
        rc::ConnectionMode mode;
        bool kept = false; // 连上了：不会过期
        QDeadlineTimer expires;
    };

    // 来连本机的又来了信令：记下（又）从哪条路来、怎么连
    void incoming(const QString &session, bool lan, const rc::ConnectionMode &mode);
    void forget();

    std::function<QString(const QString &)> nearbyAddress_;
    QHash<QString, Outgoing> viewerSessions_;
    QHash<QString, Incoming> sharerSessions_;
    QTimer forgetTimer_;
};
