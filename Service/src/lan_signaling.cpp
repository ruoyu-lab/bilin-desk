#include "lan_signaling.h"

#include "device_store.h"

#include <rc/device_id.h>
#include <rc/line_channel.h>
#include <rc/transport.h>

#include <QTcpSocket>

#include <algorithm>

namespace {

QString peerAddress(const QTcpSocket *socket)
{
    // 只要 IPv4 的样子，不要 ::ffff:a.b.c.d
    const QHostAddress address = socket->peerAddress();
    bool isIPv4 = false;
    const quint32 ipv4 = address.toIPv4Address(&isIPv4);
    return isIPv4 ? QHostAddress(ipv4).toString() : address.toString();
}

} // namespace

LanSignaling::LanSignaling(DeviceStore &store, Options options, QObject *parent)
    : QObject(parent)
    , store_(store)
    , options_(options)
{
    connect(&server_, &QTcpServer::newConnection, this, &LanSignaling::accept);
}

bool LanSignaling::listen()
{
    if (server_.listen(QHostAddress::Any, options_.listenPort))
        return true;
    qWarning("局域网信令：端口 %d 用不了（%s），同一网络里的电脑没法不经服务器连本机", options_.listenPort,
             qUtf8Printable(server_.errorString()));
    return false;
}

void LanSignaling::send(const QString &address, const QString &session, const QString &text,
                        const rc::ConnectionMode &mode)
{
    const auto device = store_.state();
    const QJsonObject message = rc::lan::signal(device.deviceId, device.deviceName, session, text, mode);
    auto link = outgoing_.find(session);
    if (link != outgoing_.end() && link->address == address) {
        if (link->channel->isConnected())
            link->channel->send(message);
        else
            link->pending.append(message);
        return;
    }
    if (link != outgoing_.end())
        drop(link->channel);

    auto *socket = new QTcpSocket;
    auto *channel = new rc::LineChannel(socket, this);
    outgoing_.insert(session, {channel, address, {message}});
    connect(socket, &QTcpSocket::connected, this, [this, channel] {
        for (Link &link : outgoing_) {
            if (link.channel != channel)
                continue;
            for (const QJsonObject &pending : std::as_const(link.pending))
                channel->send(pending);
            link.pending.clear();
        }
    });
    connect(channel, &rc::LineChannel::received, this,
            [this, channel, address](const QJsonObject &message) { handle(channel, address, message); });
    connect(channel, &rc::LineChannel::connectFailed, this, [this, channel, session, address] {
        qInfo("局域网里连不上 %s", qUtf8Printable(address));
        drop(channel);
        emit unreachable(session);
    });
    connect(channel, &rc::LineChannel::disconnected, this, [this, channel] { drop(channel); });
    socket->connectToHost(address, options_.peerPort);
}

void LanSignaling::answer(const QString &session, const QString &text, const rc::ConnectionMode &mode)
{
    const auto link = incoming_.constFind(session);
    if (link == incoming_.cend())
        return;
    const auto device = store_.state();
    link->channel->send(rc::lan::signal(device.deviceId, device.deviceName, session, text, mode));
}

void LanSignaling::decline(const QString &session, const QString &reason)
{
    const auto link = incoming_.constFind(session);
    if (link == incoming_.cend())
        return;
    link->channel->send(rc::lan::decline(store_.state().deviceId, session, reason));
}

void LanSignaling::accept()
{
    while (QTcpSocket *socket = server_.nextPendingConnection()) {
        const QString address = peerAddress(socket);
        auto *channel = new rc::LineChannel(socket, this);
        connect(channel, &rc::LineChannel::received, this,
                [this, channel, address](const QJsonObject &message) { handle(channel, address, message); });
        connect(channel, &rc::LineChannel::disconnected, this, [this, channel] { drop(channel); });
    }
}

void LanSignaling::handle(rc::LineChannel *channel, const QString &address, const QJsonObject &message)
{
    const QString type = message.value(QLatin1StringView("type")).toString();
    const QString from = message.value(QLatin1StringView("from")).toString();
    const QString session = message.value(QLatin1StringView("session")).toString();
    if (!rc::DeviceId::parse(from).isValid() || session.isEmpty()) {
        qWarning("局域网信令：%s 发来的消息不对，断开", qUtf8Printable(address));
        channel->close();
        return;
    }
    if (type == rc::lan::Decline) {
        emit declined(from, session, message.value(QLatin1StringView("reason")).toString());
        return;
    }
    if (type != rc::lan::Signal)
        return;
    const std::optional<rc::ConnectionMode> mode = rc::ConnectionMode::read(message);
    if (!mode) {
        qWarning("局域网信令：%s 发来的信令不对（怎么连），断开", qUtf8Printable(address));
        channel->close();
        return;
    }
    // 别人来连本机：记下这次连接走哪条 TCP 连接回答
    const bool outgoing = std::any_of(outgoing_.cbegin(), outgoing_.cend(),
                                      [channel](const Link &link) { return link.channel == channel; });
    if (!outgoing)
        incoming_.insert(session, {channel, address, {}});
    emit received(address, from, message.value(QLatin1StringView("name")).toString(), session,
                  message.value(QLatin1StringView("text")).toString(), *mode);
}

void LanSignaling::drop(rc::LineChannel *channel)
{
    outgoing_.removeIf([channel](const auto &entry) { return entry.value().channel == channel; });
    incoming_.removeIf([channel](const auto &entry) { return entry.value().channel == channel; });
    channel->deleteLater();
}

void LanSignaling::close(const QString &session)
{
    if (const auto link = outgoing_.constFind(session); link != outgoing_.cend())
        link->channel->close();
    if (const auto link = incoming_.constFind(session); link != incoming_.cend())
        link->channel->close();
}
