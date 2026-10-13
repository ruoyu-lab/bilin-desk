#pragma once

#include <rc/protocol.h>
#include <rc/transport.h>

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <optional>

class DeviceStore;

// 和服务器之间的长连接（消息见 rc/server_protocol.h）：登记本机的设备 ID、名字、设备公钥、在线，收发信令；
// 本机登录的账号和账号下的设备（account()，服务器有变化时发来）；P2P 和中继用的服务器（ice()，登录时发来，
// 中继的凭据快过期时服务器再发新的；断着连接到了期就不能再用中继）。
// - 断了就重连，等 1、2、4……秒，最多 30 秒；登录成功后从头算
// - 每 30 秒 ping 一次，75 秒没听到回应就当断了（睡眠醒来、网络换了，TCP 自己不一定知道）
// - 服务器说 ID 已经属于别的设备：换一个新 ID 马上再登记
// - 服务器说这台设备从别处又登录了：这次运行不再重连，免得两边互相把对方挤下去
// - wss:// 带着证书时只认这一张（服务器自己签的，没有域名）：系统信任的证书机构都不算数，别的证书一律不连
class ServerLink : public QObject
{
    Q_OBJECT

public:
    // Connecting：启动后第一次连接、登录 | Online | Offline：断了、连不上（重连期间一直是它，登录上才变），
    // 或者被别处挤下去了
    enum class State { Connecting, Online, Offline };

    struct Options
    {
        int firstRetryMs = 1000;
        int maxRetryMs = 30000;
        int pingMs = 30000;
        int silenceMs = 75000;
    };

    // url：服务器的长连接地址；wss:// 的证书按系统信任的证书机构校验
    ServerLink(DeviceStore &store, QUrl url, Options options, QObject *parent = nullptr);

    void start();
    State state() const { return state_; }

    // 发给设备 to 的信令（mode 是这次连接怎么连）。没登录服务器时发不出去，返回 false
    bool sendSignal(const QString &to, const QString &session, const QString &text, const rc::ConnectionMode &mode);
    // 回绝设备 to 的这次连接。没登录服务器时发不出去（对方也就等不到回答）
    void sendDecline(const QString &to, const QString &session, const QString &reason);

    // 账号。没登录服务器时发不出去，返回 false；结果是 accountChanged，或者 accountFailed
    rc::protocol::AccountState account() const { return account_; }
    // 账号下别的设备（不算本机）的公钥（65 字节）：它们按我的设备来连本机（rc/transport.h 的 DeviceAccess），
    // 本机也按我的设备去连它们。没登录账号时是空的；启动以后服务器还没告诉本机账号时不知道（nullopt）
    std::optional<QList<QByteArray>> trustedKeys() const;
    // 往邮箱发验证码（结果是 codeSent，或者 accountFailed）；purpose 见 rc/protocol.h 的 SendCode
    bool sendCode(const QString &email, const QString &purpose);
    bool registerAccount(const QString &email, const QString &password, const QString &nickname, const QString &code);
    bool signIn(const QString &email, const QString &password);
    bool resetPassword(const QString &email, const QString &code, const QString &password);
    bool signOut();
    bool removeDevice(const QString &deviceId);
    // 登录着账号时：改昵称、改密码（结果是 passwordChanged，或者 accountFailed）、注销账号
    bool setNickname(const QString &nickname);
    bool changePassword(const QString &oldPassword, const QString &newPassword);
    bool deleteAccount(const QString &password);

    rc::IceServers ice() const { return ice_; }

signals:
    void stateChanged();
    // 设备 from（名字 name）发来的信令
    void signalReceived(const QString &from, const QString &name, const QString &session, const QString &text,
                        const rc::ConnectionMode &mode);
    // 设备 from 回绝了这次连接
    void declined(const QString &from, const QString &session, const QString &reason);
    // 发给设备 to 的信令没送到；reason：offline（它不在线，或者没有这个 ID）| tooMany（服务器先不让连）
    void undeliverable(const QString &to, const QString &session, const QString &reason);
    void accountChanged();
    // reason：rc/server_protocol.h 的 AccountFailed
    void accountFailed(const QString &reason);
    void passwordChanged();
    void codeSent(const QString &purpose);
    void iceChanged();

private:
    void open();
    void sayHello();
    // 登录着服务器时发出去；没登录时 false
    bool sendOnline(const QJsonObject &message);
    // 名字改了：登录着就告诉服务器
    void deviceChanged();
    void send(const QJsonObject &message);
    void received(const QString &text);
    void socketStateChanged(QAbstractSocket::SocketState state);
    void ping();
    void setState(State state);
    void forgetPresence();
    // 服务器发来的 P2P 和中继用的服务器（rc/server_protocol.h 的 Welcome、Ice）
    void setIce(const QJsonObject &json);
    void turnExpired();

    DeviceStore &store_;
    const QUrl url_;
    const QString machine_; // rc::machineHash()
    const Options options_;
    QWebSocket socket_;
    QTimer retryTimer_;
    QTimer pingTimer_;
    QTimer turnExpiry_; // 中继的凭据到期
    QElapsedTimer heard_;
    State state_ = State::Connecting;
    QString nameSent_; // 服务器记着的本机名字
    rc::protocol::AccountState account_;
    // 启动以后服务器告诉过本机账号了：没登录账号，或者账号下的设备（登录上以后才发，比 welcome 晚）
    bool accountKnown_ = false;
    rc::IceServers ice_;
    int retryMs_;
    bool reconnectNow_ = false; // 换了 ID：连接关掉后马上再连
    bool stopped_ = false;      // 被别处挤下去了：不再重连
};
