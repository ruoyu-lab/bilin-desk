#include "device_wake.h"

#include <rc/device_id.h>
#include <rc/json_file.h>

#include <QHostAddress>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <cstring>

namespace {

DeviceWake *current = nullptr;

// 一个包可能丢：隔一秒发一个，共三个
constexpr int Packets = 3;
constexpr int PacketGapMs = 1000;

QString macText(const uint8_t (&mac)[6])
{
    return QByteArray(reinterpret_cast<const char *>(mac), 6).toHex(':');
}

bool readMac(const QString &text, uint8_t (&mac)[6])
{
    const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
    if (bytes.size() != 6)
        return false;
    std::memcpy(mac, bytes.constData(), 6);
    return true;
}

QJsonObject toJson(const rcs_power_interface &item)
{
    return {
        {QStringLiteral("mac"), macText(item.mac)},
        {QStringLiteral("link"), int(item.link)},
        {QStringLiteral("wake"), int(item.wake)},
        {QStringLiteral("address"), QString::fromLatin1(item.address)},
        {QStringLiteral("prefix"), int(item.prefix_length)},
        {QStringLiteral("broadcast"), QString::fromLatin1(item.broadcast)},
    };
}

std::optional<rcs_power_interface> fromJson(const QJsonObject &json)
{
    rcs_power_interface item{};
    if (!readMac(json.value(QLatin1StringView("mac")).toString(), item.mac))
        return std::nullopt;
    item.link = rcs_power_link(json.value(QLatin1StringView("link")).toInt());
    item.wake = rcs_power_wake_setting(json.value(QLatin1StringView("wake")).toInt());
    item.prefix_length = uint32_t(json.value(QLatin1StringView("prefix")).toInt());
    qstrncpy(item.address, json.value(QLatin1StringView("address")).toString().toLatin1().constData(),
             sizeof item.address);
    qstrncpy(item.broadcast, json.value(QLatin1StringView("broadcast")).toString().toLatin1().constData(),
             sizeof item.broadcast);
    return item;
}

bool same(const rcs_power_interface &a, const rcs_power_interface &b)
{
    return std::memcmp(a.mac, b.mac, 6) == 0 && a.link == b.link && a.wake == b.wake && a.prefix_length == b.prefix_length
           && std::strcmp(a.address, b.address) == 0 && std::strcmp(a.broadcast, b.broadcast) == 0;
}

// 本机的网卡（有 IPv4 地址的）
QList<rcs_power_interface> localInterfaces()
{
    std::vector<rcs_power_interface> items(RCS_POWER_INTERFACES_MAX);
    uint32_t count = 0;
    if (rcs_power_list_interfaces(items.data(), sizeof(rcs_power_interface), uint32_t(items.size()), &count) != RCS_OK)
        return {};
    return QList<rcs_power_interface>(items.begin(), items.begin() + std::min<size_t>(count, items.size()));
}

// 两个地址在不在 prefix 这个网段里
bool sameSubnet(const char *a, const char *b, uint32_t prefix)
{
    return QHostAddress(QString::fromLatin1(a)).isInSubnet(QHostAddress(QString::fromLatin1(b)), int(prefix));
}

QString digits(const QString &deviceId)
{
    return rc::DeviceId::parse(deviceId).digits();
}

} // namespace

DeviceWake::DeviceWake(QString file, QObject *parent)
    : QObject(parent)
    , file_(std::move(file))
{
    load();
}

void DeviceWake::setInstance(DeviceWake *wake)
{
    current = wake;
}

DeviceWake *DeviceWake::instance()
{
    return current;
}

DeviceWake *DeviceWake::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(current);
    QJSEngine::setObjectOwnership(current, QJSEngine::CppOwnership);
    return current;
}

void DeviceWake::remember(const QString &deviceId, const QList<rcs_power_interface> &interfaces)
{
    const QList<rcs_power_interface> known = devices_.value(deviceId);
    if (known.size() == interfaces.size()
        && std::equal(known.cbegin(), known.cend(), interfaces.cbegin(), same))
        return;
    if (interfaces.isEmpty())
        devices_.remove(deviceId);
    else
        devices_.insert(deviceId, interfaces);
    save();
    ++revision_;
    emit changed();
}

QString DeviceWake::problem(const QString &deviceId) const
{
    const QList<rcs_power_interface> known = devices_.value(digits(deviceId));
    if (known.isEmpty())
        return tr("还不知道它的网卡：连上它一次以后就知道了");
    if (std::all_of(known.cbegin(), known.cend(),
                    [](const rcs_power_interface &item) { return item.wake == RCS_POWER_WAKE_DISABLED; }))
        return tr("它没有打开网络唤醒");
    if (reachable(deviceId).isEmpty())
        return tr("本机和它不在同一个网络里，叫不醒它");
    return {};
}

bool DeviceWake::wake(const QString &deviceId)
{
    const QList<rcs_power_interface> targets = reachable(deviceId);
    if (targets.isEmpty())
        return false;
    qInfo("唤醒 %s：往 %lld 块网卡发唤醒包", qUtf8Printable(digits(deviceId)), qlonglong(targets.size()));
    for (int packet = 0; packet < Packets; ++packet) {
        QTimer::singleShot(packet * PacketGapMs, this, [targets] {
            for (const rcs_power_interface &target : targets) {
                rcs_power_wake wake = rcs_power_wake_init();
                std::memcpy(wake.mac, target.mac, 6);
                wake.address = target.broadcast;
                if (const rcs_result result = rcs_power_send_wake(&wake); result != RCS_OK)
                    qWarning("唤醒包没发出去（%s）：%s", macText(target.mac).toLatin1().constData(),
                             rcs_result_message(result));
            }
        });
    }
    return true;
}

QList<rcs_power_interface> DeviceWake::reachable(const QString &deviceId) const
{
    const QList<rcs_power_interface> local = localInterfaces();
    QList<rcs_power_interface> targets;
    for (const rcs_power_interface &target : devices_.value(digits(deviceId))) {
        if (target.wake == RCS_POWER_WAKE_DISABLED)
            continue;
        if (std::any_of(local.cbegin(), local.cend(), [&target](const rcs_power_interface &mine) {
                return sameSubnet(mine.address, target.address, target.prefix_length);
            }))
            targets.append(target);
    }
    return targets;
}

void DeviceWake::load()
{
    const QJsonObject json = rc::readJsonFile(file_);
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        QList<rcs_power_interface> interfaces;
        for (const QJsonValue &value : it.value().toArray()) {
            if (const std::optional<rcs_power_interface> item = fromJson(value.toObject()))
                interfaces.append(*item);
        }
        if (!interfaces.isEmpty())
            devices_.insert(it.key(), interfaces);
    }
}

void DeviceWake::save() const
{
    QJsonObject json;
    for (auto it = devices_.constBegin(); it != devices_.constEnd(); ++it) {
        QJsonArray array;
        for (const rcs_power_interface &item : it.value())
            array.append(toJson(item));
        json.insert(it.key(), array);
    }
    rc::writeJsonFile(file_, json);
}
