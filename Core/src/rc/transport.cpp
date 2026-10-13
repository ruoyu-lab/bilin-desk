#include "rc/transport.h"

#include <QJsonArray>

namespace rc {

bool isTransport(QStringView name)
{
    return name == DirectTransport || name == RelayTransport;
}

bool isAccess(QStringView name)
{
    return name == PasswordAccess || name == DeviceAccess;
}

QString sdkVersion()
{
    return QStringLiteral(RC_SDK_VERSION);
}

void ConnectionMode::write(QJsonObject &message) const
{
    message.insert(QStringLiteral("transport"), transport);
    message.insert(QStringLiteral("access"), access);
    message.insert(QStringLiteral("sdk"), sdk);
}

std::optional<ConnectionMode> ConnectionMode::read(const QJsonObject &message)
{
    ConnectionMode mode{message.value(QLatin1StringView("transport")).toString(),
                        message.value(QLatin1StringView("access")).toString(),
                        message.value(QLatin1StringView("sdk")).toString()};
    if (!isTransport(mode.transport) || !isAccess(mode.access) || mode.sdk.isEmpty())
        return std::nullopt;
    return mode;
}

std::vector<std::string> IceServers::stunServers() const
{
    std::vector<std::string> servers;
    for (const QString &server : stun)
        servers.push_back(server.toStdString());
    return servers;
}

QJsonObject IceServers::toJson() const
{
    QJsonValue relay;
    if (turn) {
        relay = QJsonObject{
            {QStringLiteral("server"), turn->server},
            {QStringLiteral("transport"), turn->tcp ? QStringLiteral("tcp") : QStringLiteral("udp")},
            {QStringLiteral("username"), turn->username},
            {QStringLiteral("password"), turn->password},
        };
    }
    return {
        {QStringLiteral("stun"), QJsonArray::fromStringList(stun)},
        {QStringLiteral("turn"), relay},
    };
}

IceServers IceServers::fromJson(const QJsonObject &json)
{
    IceServers ice;
    for (const QJsonValue &server : json.value(QLatin1StringView("stun")).toArray())
        ice.stun.append(server.toString());
    const QJsonObject relay = json.value(QLatin1StringView("turn")).toObject();
    if (!relay.isEmpty()) {
        ice.turn = TurnServer{relay.value(QLatin1StringView("server")).toString(),
                              relay.value(QLatin1StringView("transport")).toString() == u"tcp",
                              relay.value(QLatin1StringView("username")).toString(),
                              relay.value(QLatin1StringView("password")).toString()};
    }
    return ice;
}

} // namespace rc
