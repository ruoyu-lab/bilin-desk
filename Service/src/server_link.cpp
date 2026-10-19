#include "server_link.h"

#include "device_store.h"

#include <rc/machine.h>
#include <rc/server_protocol.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace server = rc::server;

ServerLink::ServerLink(DeviceStore &store, QUrl url, Options options, QObject *parent)
    : QObject(parent)
    , store_(store)
    , url_(std::move(url))
    , machine_(rc::machineHash())
    , options_(options)
    , retryMs_(options.firstRetryMs)
{
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, &ServerLink::open);
    pingTimer_.setInterval(options_.pingMs);
    connect(&pingTimer_, &QTimer::timeout, this, &ServerLink::ping);
    turnExpiry_.setSingleShot(true);
    connect(&turnExpiry_, &QTimer::timeout, this, &ServerLink::turnExpired);

    // 证书不对（过期、不是这个域名的、系统不信任）：不连，记下为什么
    connect(&socket_, &QWebSocket::sslErrors, this, [](const QList<QSslError> &errors) {
        qWarning("服务器的证书不对，不连：%s", qUtf8Printable(errors.value(0).errorString()));
    });
    connect(&socket_, &QWebSocket::connected, this, &ServerLink::sayHello);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &ServerLink::received);
    connect(&socket_, &QWebSocket::pong, this, [this] { heard_.restart(); });
    // 没连上和断了都以"回到未连接"为准
    connect(&socket_, &QWebSocket::stateChanged, this, &ServerLink::socketStateChanged);
    connect(&store_, &DeviceStore::changed, this, &ServerLink::deviceChanged);
}

void ServerLink::start()
{
    qInfo("服务器：%s", qUtf8Printable(url_.toString()));
    open();
}

void ServerLink::open()
{
    socket_.open(url_);
}

void ServerLink::sayHello()
{
    heard_.restart();
    const auto device = store_.state();
    nameSent_ = device.deviceName;
    send(server::hello(device.deviceId, store_.deviceToken(), store_.publicKey(), device.deviceName,
                       QCoreApplication::applicationVersion(), machine_));
}

void ServerLink::deviceChanged()
{
    const QString name = store_.state().deviceName;
    if (state_ != State::Online || name == nameSent_)
        return;
    nameSent_ = name;
    send(server::rename(name));
}

void ServerLink::send(const QJsonObject &message)
{
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

bool ServerLink::sendSignal(const QString &to, const QString &session, const QString &text,
                            const rc::ConnectionMode &mode)
{
    if (state_ != State::Online)
        return false;
    send(server::signal(to, session, text, mode));
    return true;
}

void ServerLink::sendWebReady(const QString &ticket, const QString &reason)
{
    sendOnline({{QStringLiteral("type"), QStringLiteral("webReady")},
                {QStringLiteral("ticket"), ticket},
                {QStringLiteral("reason"), reason}});
}

void ServerLink::sendDecline(const QString &to, const QString &session, const QString &reason)
{
    if (state_ == State::Online)
        send(server::decline(to, session, reason));
}

bool ServerLink::sendOnline(const QJsonObject &message)
{
    if (state_ != State::Online)
        return false;
    send(message);
    return true;
}

bool ServerLink::sendCode(const QString &email, const QString &purpose)
{
    return sendOnline(server::sendCode(email, purpose));
}

bool ServerLink::registerAccount(const QString &email, const QString &password, const QString &nickname,
                                 const QString &code)
{
    return sendOnline(server::registerAccount(email, password, nickname, code));
}

bool ServerLink::signIn(const QString &email, const QString &password)
{
    return sendOnline(server::signIn(email, password));
}

bool ServerLink::resetPassword(const QString &email, const QString &code, const QString &password)
{
    return sendOnline(server::resetPassword(email, code, password));
}

bool ServerLink::signOut()
{
    return sendOnline(server::signOut());
}

bool ServerLink::removeDevice(const QString &deviceId)
{
    return sendOnline(server::removeDevice(deviceId));
}

bool ServerLink::setNickname(const QString &nickname)
{
    return sendOnline(server::setNickname(nickname));
}

bool ServerLink::changePassword(const QString &oldPassword, const QString &newPassword)
{
    return sendOnline(server::changePassword(oldPassword, newPassword));
}

bool ServerLink::deleteAccount(const QString &password)
{
    return sendOnline(server::deleteAccount(password));
}

void ServerLink::received(const QString &text)
{
    heard_.restart();
    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = message.value(QLatin1StringView("type")).toString();
    if (type == server::Welcome) {
        qInfo("已登录服务器");
        retryMs_ = options_.firstRetryMs;
        pingTimer_.start();
        setIce(message.value(QLatin1StringView("ice")).toObject());
        setState(State::Online);
    } else if (type == server::Ice) {
        setIce(message.value(QLatin1StringView("ice")).toObject());
    } else if (type == server::Rejected) {
        const QString reason = message.value(QLatin1StringView("reason")).toString();
        if (reason == server::IdTaken) {
            store_.replaceDeviceId();
            reconnectNow_ = true;
        } else {
            qWarning("服务器不让登录：%s", qUtf8Printable(reason));
        }
        // 服务器随后会关掉连接
    } else if (type == server::Renumbered) {
        store_.takeDeviceId(message.value(QLatin1StringView("deviceId")).toString());
        reconnectNow_ = true;
        // 服务器随后会关掉连接
    } else if (type == server::Signal) {
        if (const std::optional<rc::ConnectionMode> mode = rc::ConnectionMode::read(message))
            emit signalReceived(message.value(QLatin1StringView("from")).toString(),
                                message.value(QLatin1StringView("name")).toString(),
                                message.value(QLatin1StringView("session")).toString(),
                                message.value(QLatin1StringView("text")).toString(), *mode);
    } else if (type == server::Decline) {
        emit declined(message.value(QLatin1StringView("from")).toString(),
                      message.value(QLatin1StringView("session")).toString(),
                      message.value(QLatin1StringView("reason")).toString());
    } else if (type == server::WebViewer) {
        const QByteArray key = QByteArray::fromBase64(message.value(QLatin1StringView("key")).toString().toLatin1());
        if (!key.isEmpty() && key.size() != 32) {
            qWarning("服务器发来的网页版钥匙不对");
            return;
        }
        emit webViewerRequested(message.value(QLatin1StringView("ticket")).toString(), key);
    } else if (type == server::Account) {
        const QJsonObject account = message.value(QLatin1StringView("account")).toObject();
        rc::protocol::AccountState state;
        state.known = true;
        state.signedIn = !account.isEmpty();
        state.email = account.value(QLatin1StringView("email")).toString();
        state.nickname = account.value(QLatin1StringView("nickname")).toString();
        // 还是同一个账号时设备列表先留着，devices 随后就到
        if (state.signedIn && state.email == account_.email)
            state.devices = account_.devices;
        // 没登录账号的这就知道了，登录了的等 devices
        const bool known = accountKnown_ || !state.signedIn;
        if (state != account_ || known != accountKnown_) {
            account_ = state;
            accountKnown_ = known;
            emit accountChanged();
        }
    } else if (type == server::Devices) {
        QList<rc::protocol::AccountDevice> devices;
        for (const QJsonValue &device : message.value(QLatin1StringView("devices")).toArray())
            devices.append(rc::protocol::AccountDevice::fromJson(device.toObject()));
        if (devices != account_.devices || !accountKnown_) {
            account_.devices = devices;
            accountKnown_ = true;
            emit accountChanged();
        }
    } else if (type == server::AccountFailed) {
        emit accountFailed(message.value(QLatin1StringView("reason")).toString());
    } else if (type == server::PasswordChanged) {
        emit passwordChanged();
    } else if (type == server::CodeSent) {
        emit codeSent(message.value(QLatin1StringView("purpose")).toString());
    } else if (type == server::Undeliverable) {
        emit undeliverable(message.value(QLatin1StringView("to")).toString(),
                           message.value(QLatin1StringView("session")).toString(),
                           message.value(QLatin1StringView("reason")).toString());
    } else if (type == server::Replaced) {
        qWarning("这台设备的 ID 在别处登录了：这次不再连服务器");
        stopped_ = true;
    } else {
        qWarning("服务器发来不认识的消息：%s", qUtf8Printable(type));
    }
}

void ServerLink::socketStateChanged(QAbstractSocket::SocketState state)
{
    if (state != QAbstractSocket::UnconnectedState)
        return;
    pingTimer_.stop();
    if (stopped_) {
        setState(State::Offline);
        return;
    }
    if (reconnectNow_) {
        reconnectNow_ = false;
        open();
        return;
    }
    if (state_ == State::Online)
        qInfo("和服务器断开了：%s", qUtf8Printable(socket_.errorString()));
    setState(State::Offline);
    retryTimer_.start(retryMs_);
    retryMs_ = std::min(retryMs_ * 2, options_.maxRetryMs);
}

void ServerLink::ping()
{
    if (heard_.elapsed() > options_.silenceMs) {
        qWarning("服务器 %d 秒没回应，重连", int(heard_.elapsed() / 1000));
        socket_.abort();
        return;
    }
    socket_.ping();
}

void ServerLink::setState(State state)
{
    if (state == state_)
        return;
    state_ = state;
    if (state_ == State::Offline)
        forgetPresence();
    emit stateChanged();
}

std::optional<QList<QByteArray>> ServerLink::trustedKeys() const
{
    if (!accountKnown_)
        return std::nullopt;
    const QString self = store_.state().deviceId;
    QList<QByteArray> keys;
    for (const rc::protocol::AccountDevice &device : account_.devices) {
        const QByteArray key = QByteArray::fromBase64(device.publicKey.toLatin1());
        if (device.deviceId != self && key.size() == DeviceStore::PublicKeySize)
            keys.append(key);
    }
    return keys;
}

// 连不上服务器就不知道账号下别的设备在不在线：都算不在线，连上以后服务器再发
void ServerLink::forgetPresence()
{
    bool changed = false;
    for (rc::protocol::AccountDevice &device : account_.devices) {
        changed = changed || device.online;
        device.online = false;
    }
    if (changed)
        emit accountChanged();
}

void ServerLink::setIce(const QJsonObject &json)
{
    const rc::IceServers ice = rc::IceServers::fromJson(json);
    if (ice.turn)
        turnExpiry_.start(std::chrono::seconds(json.value(QLatin1StringView("ttl")).toInt()));
    else
        turnExpiry_.stop();
    if (ice == ice_)
        return;
    ice_ = ice;
    qInfo("中继：%s", ice_.turn ? qUtf8Printable(ice_.turn->server) : "没有");
    emit iceChanged();
}

// 一直没连上服务器，没拿到新的凭据：中继不能用了，连上以后再拿
void ServerLink::turnExpired()
{
    qInfo("中继的凭据过期了");
    ice_.turn.reset();
    emit iceChanged();
}
