#include "rc/lan_protocol.h"

#include "rc/device_id.h"

#include <QHostAddress>
#include <QJsonDocument>

namespace rc::lan {

QByteArray announce(const Presence &presence)
{
    QJsonObject message{
        {QStringLiteral("app"), QString(AppTag)},
        {QStringLiteral("type"), QString(Announce)},
        {QStringLiteral("deviceId"), presence.deviceId},
        {QStringLiteral("name"), presence.name},
        {QStringLiteral("platform"), presence.platform},
    };
    if (!presence.address.isEmpty())
        message.insert(QStringLiteral("address"), presence.address);
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray leave(const QString &deviceId)
{
    const QJsonObject message{
        {QStringLiteral("app"), QString(AppTag)},
        {QStringLiteral("type"), QString(Leave)},
        {QStringLiteral("deviceId"), deviceId},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

std::optional<Presence> parsePresence(const QByteArray &datagram)
{
    const QJsonObject message = QJsonDocument::fromJson(datagram).object();
    if (message.value(QLatin1StringView("app")).toString() != AppTag)
        return std::nullopt;
    Presence presence;
    presence.deviceId = message.value(QLatin1StringView("deviceId")).toString();
    if (!DeviceId::parse(presence.deviceId).isValid())
        return std::nullopt;
    const QString type = message.value(QLatin1StringView("type")).toString();
    if (type == Leave)
        return presence;
    if (type != Announce)
        return std::nullopt;
    presence.name = message.value(QLatin1StringView("name")).toString();
    presence.platform = message.value(QLatin1StringView("platform")).toString();
    // 只认 IPv4 地址，别的当没说
    const QHostAddress address(message.value(QLatin1StringView("address")).toString());
    if (address.protocol() == QAbstractSocket::IPv4Protocol)
        presence.address = address.toString();
    if (presence.name.isEmpty())
        return std::nullopt;
    return presence;
}

QJsonObject signal(const QString &from, const QString &name, const QString &session, const QString &text,
                   const rc::ConnectionMode &mode)
{
    QJsonObject message{
        {QStringLiteral("type"), QString(Signal)},
        {QStringLiteral("from"), from},
        {QStringLiteral("name"), name},
        {QStringLiteral("session"), session},
        {QStringLiteral("text"), text},
    };
    mode.write(message);
    return message;
}

QJsonObject decline(const QString &from, const QString &session, const QString &reason)
{
    return {
        {QStringLiteral("type"), QString(Decline)},
        {QStringLiteral("from"), from},
        {QStringLiteral("session"), session},
        {QStringLiteral("reason"), reason},
    };
}

} // namespace rc::lan
