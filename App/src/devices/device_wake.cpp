#include "device_wake.h"

#include <rc/device_id.h>
#include <rc/json_file.h>

#include <QHostAddress>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInterface>
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

// 本机现在的 IPv4 地址（连着的网卡的）。只用来看和它在不在同一个网段：Qt 在各个系统上都拿得到，
// SDK 的 rcs_power_list_interfaces 在手机上没有（Windows 上还要一会儿，不该在界面线程上问）
QList<QHostAddress> localAddresses()
{
    QList<QHostAddress> addresses;
    for (const QNetworkInterface &network : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = network.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : network.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                addresses.append(entry.ip());
        }
    }
    return addresses;
}

// 本机的地址在不在它这块网卡的网段里
bool inSubnet(const QHostAddress &mine, const rcs_power_interface &target)
{
    return mine.isInSubnet(QHostAddress(QString::fromLatin1(target.address)), int(target.prefix_length));
}

QString digits(const QString &deviceId)
{
    return rc::DeviceId::parse(deviceId).digits();
}

// 往每块网卡发一个唤醒包；有一个发出去了就算成（结果是最后一个没发出去的原因）
rcs_result sendWakes(const QList<rcs_power_interface> &targets)
{
    rcs_result failed = RCS_OK;
    bool sent = false;
    for (const rcs_power_interface &target : targets) {
        rcs_power_wake wake = rcs_power_wake_init();
        std::memcpy(wake.mac, target.mac, 6);
        wake.address = target.broadcast;
        if (const rcs_result result = rcs_power_send_wake(&wake); result != RCS_OK) {
            qWarning("唤醒包没发出去（%s）：%s", macText(target.mac).toLatin1().constData(), rcs_result_message(result));
            failed = result;
        } else {
            sent = true;
        }
    }
    return sent ? RCS_OK : failed;
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
    return unsent_.value(digits(deviceId));
}

bool DeviceWake::wake(const QString &deviceId)
{
    const QList<rcs_power_interface> targets = reachable(deviceId);
    if (targets.isEmpty())
        return false;
    const QString key = digits(deviceId);
    qInfo("唤醒 %s：往 %lld 块网卡发唤醒包", qUtf8Printable(key), qlonglong(targets.size()));
    // 第一个现在发：系统不让发（iPhone、iPad 没有苹果给的局域网广播权限）就告诉界面，别说"已叫醒"
    const rcs_result first = sendWakes(targets);
    const QString unsent = first == RCS_OK ? QString()
                           : first == RCS_ERROR_PERMISSION_REQUIRED ? tr("系统不让本程序在局域网里发唤醒包")
                           : tr("唤醒包没发出去：%1").arg(QString::fromUtf8(rcs_result_message(first)));
    if (unsent != unsent_.value(key)) {
        if (unsent.isEmpty())
            unsent_.remove(key);
        else
            unsent_.insert(key, unsent);
        ++revision_;
        emit changed();
    }
    if (first != RCS_OK)
        return false;
    for (int packet = 1; packet < Packets; ++packet)
        QTimer::singleShot(packet * PacketGapMs, this, [targets] { sendWakes(targets); });
    return true;
}

QList<rcs_power_interface> DeviceWake::reachable(const QString &deviceId) const
{
    const QList<QHostAddress> local = localAddresses();
    QList<rcs_power_interface> targets;
    for (const rcs_power_interface &target : devices_.value(digits(deviceId))) {
        if (target.wake == RCS_POWER_WAKE_DISABLED)
            continue;
        if (std::any_of(local.cbegin(), local.cend(), [&target](const QHostAddress &mine) { return inSubnet(mine, target); }))
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
