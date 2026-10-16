#include "rc/protocol.h"

#include <QJsonArray>

namespace rc::protocol {

namespace {

QJsonArray peersToJson(const QList<SharePeer> &peers)
{
    QJsonArray array;
    for (const SharePeer &peer : peers)
        array.append(peer.toJson());
    return array;
}

QList<SharePeer> peersFromJson(const QJsonValue &value)
{
    QList<SharePeer> peers;
    for (const QJsonValue &peer : value.toArray())
        peers.append(SharePeer::fromJson(peer.toObject()));
    return peers;
}

} // namespace

QJsonObject DeviceState::toJson() const
{
    return {
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("deviceName"), deviceName},
        {QStringLiteral("password"), password},
        {QStringLiteral("acceptIncoming"), acceptIncoming},
        {QStringLiteral("lanDiscovery"), lanDiscovery},
        {QStringLiteral("unattended"), unattended},
        {QStringLiteral("deviceKey"), QString::fromLatin1(deviceKey.toBase64())},
    };
}

DeviceState DeviceState::fromJson(const QJsonObject &json)
{
    DeviceState state;
    state.deviceId = json.value(QLatin1StringView("deviceId")).toString();
    state.deviceName = json.value(QLatin1StringView("deviceName")).toString();
    state.password = json.value(QLatin1StringView("password")).toString();
    state.acceptIncoming = json.value(QLatin1StringView("acceptIncoming")).toBool();
    state.lanDiscovery = json.value(QLatin1StringView("lanDiscovery")).toBool();
    state.unattended = json.value(QLatin1StringView("unattended")).toBool();
    state.deviceKey = QByteArray::fromBase64(json.value(QLatin1StringView("deviceKey")).toString().toLatin1());
    return state;
}

QJsonObject SharePeer::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("name"), name},
        {QStringLiteral("platform"), platform},
        {QStringLiteral("control"), control},
    };
}

SharePeer SharePeer::fromJson(const QJsonObject &json)
{
    SharePeer peer;
    peer.id = json.value(QLatin1StringView("id")).toInt();
    peer.deviceId = json.value(QLatin1StringView("deviceId")).toString();
    peer.name = json.value(QLatin1StringView("name")).toString();
    peer.platform = json.value(QLatin1StringView("platform")).toString();
    peer.control = json.value(QLatin1StringView("control")).toBool();
    return peer;
}

QJsonObject ShareState::toJson() const
{
    return {
        {QStringLiteral("accepting"), accepting},
        {QStringLiteral("problem"), problem},
        {QStringLiteral("screenPermission"), screenPermission},
        {QStringLiteral("inputPermission"), inputPermission},
        {QStringLiteral("canControl"), canControl},
        {QStringLiteral("clipboard"), clipboard},
        {QStringLiteral("requests"), peersToJson(requests)},
        {QStringLiteral("viewers"), peersToJson(viewers)},
    };
}

ShareState ShareState::fromJson(const QJsonObject &json)
{
    ShareState state;
    state.accepting = json.value(QLatin1StringView("accepting")).toBool();
    state.problem = json.value(QLatin1StringView("problem")).toString();
    state.screenPermission = json.value(QLatin1StringView("screenPermission")).toBool();
    state.inputPermission = json.value(QLatin1StringView("inputPermission")).toBool();
    state.canControl = json.value(QLatin1StringView("canControl")).toBool();
    state.clipboard = json.value(QLatin1StringView("clipboard")).toBool();
    state.requests = peersFromJson(json.value(QLatin1StringView("requests")));
    state.viewers = peersFromJson(json.value(QLatin1StringView("viewers")));
    return state;
}

QJsonObject NearbyDevice::toJson() const
{
    return {
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("name"), name},
        {QStringLiteral("platform"), platform},
        {QStringLiteral("address"), address},
    };
}

NearbyDevice NearbyDevice::fromJson(const QJsonObject &json)
{
    return {json.value(QLatin1StringView("deviceId")).toString(), json.value(QLatin1StringView("name")).toString(),
            json.value(QLatin1StringView("platform")).toString(), json.value(QLatin1StringView("address")).toString()};
}

QJsonObject NearbyState::toJson() const
{
    QJsonArray array;
    for (const NearbyDevice &device : devices)
        array.append(device.toJson());
    return {{QStringLiteral("devices"), array}};
}

NearbyState NearbyState::fromJson(const QJsonObject &json)
{
    NearbyState state;
    for (const QJsonValue &device : json.value(QLatin1StringView("devices")).toArray())
        state.devices.append(NearbyDevice::fromJson(device.toObject()));
    return state;
}

QJsonObject AccountDevice::toJson() const
{
    return {
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("name"), name},
        {QStringLiteral("platform"), platform},
        {QStringLiteral("publicKey"), publicKey},
        {QStringLiteral("online"), online},
    };
}

AccountDevice AccountDevice::fromJson(const QJsonObject &json)
{
    return {json.value(QLatin1StringView("deviceId")).toString(), json.value(QLatin1StringView("name")).toString(),
            json.value(QLatin1StringView("platform")).toString(), json.value(QLatin1StringView("publicKey")).toString(),
            json.value(QLatin1StringView("online")).toBool()};
}

QJsonObject AccountState::toJson() const
{
    QJsonArray array;
    for (const AccountDevice &device : devices)
        array.append(device.toJson());
    return {
        {QStringLiteral("known"), known},
        {QStringLiteral("signedIn"), signedIn},
        {QStringLiteral("email"), email},
        {QStringLiteral("nickname"), nickname},
        {QStringLiteral("devices"), array},
    };
}

AccountState AccountState::fromJson(const QJsonObject &json)
{
    AccountState state;
    state.known = json.value(QLatin1StringView("known")).toBool();
    state.signedIn = json.value(QLatin1StringView("signedIn")).toBool();
    state.email = json.value(QLatin1StringView("email")).toString();
    state.nickname = json.value(QLatin1StringView("nickname")).toString();
    for (const QJsonValue &device : json.value(QLatin1StringView("devices")).toArray())
        state.devices.append(AccountDevice::fromJson(device.toObject()));
    return state;
}

QJsonObject UpdateState::toJson() const
{
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("phase"), phase},
        {QStringLiteral("progress"), progress},
        {QStringLiteral("failed"), failed},
        {QStringLiteral("checked"), checked},
    };
}

UpdateState UpdateState::fromJson(const QJsonObject &json)
{
    UpdateState state;
    state.available = json.value(QLatin1StringView("available")).toString();
    state.phase = json.value(QLatin1StringView("phase")).toString(QStringLiteral("idle"));
    state.progress = json.value(QLatin1StringView("progress")).toDouble();
    state.failed = json.value(QLatin1StringView("failed")).toString();
    state.checked = json.value(QLatin1StringView("checked")).toString();
    return state;
}

QJsonObject HistoryEntry::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("name"), name},
        {QStringLiteral("at"), at},
        {QStringLiteral("endedAt"), endedAt},
        {QStringLiteral("control"), control},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("file"), file},
        {QStringLiteral("count"), count},
        {QStringLiteral("bytes"), bytes},
        {QStringLiteral("sent"), sent},
        {QStringLiteral("version"), version},
        {QStringLiteral("action"), action},
    };
}

HistoryEntry HistoryEntry::fromJson(const QJsonObject &json)
{
    HistoryEntry entry;
    entry.id = json.value(QLatin1StringView("id")).toInt();
    entry.kind = json.value(QLatin1StringView("kind")).toString();
    entry.deviceId = json.value(QLatin1StringView("deviceId")).toString();
    entry.name = json.value(QLatin1StringView("name")).toString();
    entry.at = json.value(QLatin1StringView("at")).toInteger();
    entry.endedAt = json.value(QLatin1StringView("endedAt")).toInteger();
    entry.control = json.value(QLatin1StringView("control")).toBool();
    entry.reason = json.value(QLatin1StringView("reason")).toString();
    entry.file = json.value(QLatin1StringView("file")).toString();
    entry.count = json.value(QLatin1StringView("count")).toInt();
    entry.bytes = json.value(QLatin1StringView("bytes")).toInteger();
    entry.sent = json.value(QLatin1StringView("sent")).toBool();
    entry.version = json.value(QLatin1StringView("version")).toString();
    entry.action = json.value(QLatin1StringView("action")).toString();
    return entry;
}

QJsonObject HistoryState::toJson() const
{
    QJsonArray array;
    for (const HistoryEntry &entry : entries)
        array.append(entry.toJson());
    return {{QStringLiteral("entries"), array}};
}

HistoryState HistoryState::fromJson(const QJsonObject &json)
{
    HistoryState state;
    for (const QJsonValue &entry : json.value(QLatin1StringView("entries")).toArray())
        state.entries.append(HistoryEntry::fromJson(entry.toObject()));
    return state;
}

QJsonObject ServiceInfo::toJson() const
{
    return {
        {QStringLiteral("program"), program},
        {QStringLiteral("modified"), modified},
    };
}

ServiceInfo ServiceInfo::fromJson(const QJsonObject &json)
{
    ServiceInfo info;
    info.program = json.value(QLatin1StringView("program")).toString();
    info.modified = json.value(QLatin1StringView("modified")).toInteger();
    return info;
}

QJsonObject topicMessage(QLatin1StringView topic, const QJsonObject &value)
{
    return {
        {QStringLiteral("topic"), QString(topic)},
        {QStringLiteral("value"), value},
    };
}

QJsonObject commandMessage(QLatin1StringView command, const QJsonObject &arguments)
{
    QJsonObject message = arguments;
    message.insert(QStringLiteral("command"), QString(command));
    return message;
}

QJsonObject eventMessage(QLatin1StringView event, const QJsonObject &arguments)
{
    QJsonObject message = arguments;
    message.insert(QStringLiteral("event"), QString(event));
    return message;
}

} // namespace rc::protocol
