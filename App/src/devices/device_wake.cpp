#include "device_wake.h"

#include "account.h"
#include "service_client.h"

#include <rc/device_id.h>
#include <rc/json_file.h>
#include <rc/protocol.h>
#include <rc/server_protocol.h>

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
// 发出去以后等它上线多久（同会话窗口里的等待）：过了就忘掉这一次
constexpr int SentKeepMs = 3 * 60 * 1000;

const QString Asking = QStringLiteral("asking");
const QString Sent = QStringLiteral("sent");
const QString Failed = QStringLiteral("failed");

// 请别的电脑帮忙的结果（Server/src/protocol.ts 的 WakeResult，另有 noServer）没成时为什么
QString failureNote(const QString &result)
{
    if (result == u"online")
        return DeviceWake::tr("它已经在线了");
    if (result == u"noInfo")
        return DeviceWake::tr("它还没告诉服务器网卡：要装上新版本、登录着账号上线一次");
    if (result == u"disabled")
        return DeviceWake::tr("它没有打开网络唤醒");
    if (result == u"noHelper")
        return DeviceWake::tr("账号下没有开着的电脑能帮忙：开着的要更新到新版本");
    if (result == u"elsewhere")
        return DeviceWake::tr("账号下开着的电脑都不和它在同一个网络里");
    if (result == u"failed")
        return DeviceWake::tr("帮忙的电脑发不出唤醒包");
    if (result == u"tooMany")
        return DeviceWake::tr("叫得太频繁了，过一会儿再试");
    if (result == rc::protocol::NoServer)
        return DeviceWake::tr("没连上服务器，过一会儿再试");
    return DeviceWake::tr("没叫成（%1）").arg(result);
}

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

DeviceWake::DeviceWake(ServiceClient &service, Account &account, QString file, QObject *parent)
    : QObject(parent)
    , service_(service)
    , account_(account)
    , file_(std::move(file))
{
    load();
    connect(&account_, &Account::changed, this, &DeviceWake::accountChanged);
    connect(&service_, &ServiceClient::eventReceived, this, [this](const QString &name, const QJsonObject &arguments) {
        if (name == rc::protocol::WakeResult)
            answered(arguments.value(QLatin1StringView("deviceId")).toString(),
                     arguments.value(QLatin1StringView("result")).toString(),
                     arguments.value(QLatin1StringView("helper")).toString());
    });
    // 等回答的时候后台服务断了：回答不会来了
    connect(&service_, &ServiceClient::stateChanged, this, [this] {
        if (service_.state() == ServiceClient::State::Ready)
            return;
        for (auto it = attempts_.cbegin(); it != attempts_.cend(); ++it) {
            if (it->state == Asking)
                QMetaObject::invokeMethod(this, [this, key = it.key()] { answered(key, rc::protocol::NoServer, {}); },
                                          Qt::QueuedConnection);
        }
    });
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
    const QString key = digits(deviceId);
    if (!reachable(key).isEmpty() || helpable(key))
        return {};
    const QList<rcs_power_interface> known = devices_.value(key);
    const auto devices = account_.otherDevices();
    const auto told = std::find_if(devices.cbegin(), devices.cend(),
                                   [&key](const rc::protocol::AccountDevice &d) { return d.deviceId == key; });
    const QString server = told == devices.cend() ? QString() : told->wake;
    if (server == u"disabled"
        || (!known.isEmpty() && std::all_of(known.cbegin(), known.cend(), [](const rcs_power_interface &item) {
               return item.wake == RCS_POWER_WAKE_DISABLED;
           })))
        return tr("它没有打开网络唤醒");
    if (server == u"ready")
        return tr("本机和它不在同一个网络里，账号下也没有开着的电脑能帮忙叫醒它");
    if (known.isEmpty())
        return tr("还不知道它的网卡：它装上新版本、登录着账号上线一次以后就知道了");
    return tr("本机和它不在同一个网络里，叫不醒它");
}

bool DeviceWake::wake(const QString &deviceId)
{
    const QString key = digits(deviceId);
    if (!problem(key).isEmpty())
        return false;
    const QList<rcs_power_interface> targets = reachable(key);
    if (!targets.isEmpty()) {
        qInfo("唤醒 %s：往 %lld 块网卡发唤醒包", qUtf8Printable(key), qlonglong(targets.size()));
        // 第一个现在发：系统不让发（iPhone、iPad 没有苹果给的局域网广播权限）就请别的电脑帮忙，别说"已叫醒"
        const rcs_result first = sendWakes(targets);
        if (first == RCS_OK) {
            for (int packet = 1; packet < Packets; ++packet)
                QTimer::singleShot(packet * PacketGapMs, this, [targets] { sendWakes(targets); });
            setAttempt(key, Sent, tr("已从本机发出唤醒包"));
            return true;
        }
        if (!helpable(key)) {
            setAttempt(key, Failed, first == RCS_ERROR_PERMISSION_REQUIRED
                                        ? tr("系统不让本程序在局域网里发唤醒包")
                                        : tr("唤醒包没发出去：%1").arg(QString::fromUtf8(rcs_result_message(first))));
            return false;
        }
    }
    qInfo("唤醒 %s：请账号下开着的电脑帮忙", qUtf8Printable(key));
    service_.send(rc::protocol::Wake, {{QStringLiteral("deviceId"), key}});
    setAttempt(key, Asking, tr("正在请账号下开着的电脑帮忙叫醒…"));
    return true;
}

QString DeviceWake::state(const QString &deviceId) const
{
    return attempts_.value(digits(deviceId)).state;
}

QString DeviceWake::status(const QString &deviceId) const
{
    const QString key = digits(deviceId);
    if (const auto attempt = attempts_.constFind(key); attempt != attempts_.cend())
        return attempt->note;
    if (const QString why = problem(key); !why.isEmpty())
        return why;
    return reachable(key).isEmpty() ? tr("睡眠、关机以后能请账号下开着的电脑帮忙叫醒")
                                    : tr("睡眠、关机以后能从本机叫醒");
}

bool DeviceWake::helpable(const QString &deviceId) const
{
    if (!account_.signedIn())
        return false;
    const auto devices = account_.otherDevices();
    const auto target = std::find_if(devices.cbegin(), devices.cend(),
                                     [&deviceId](const rc::protocol::AccountDevice &d) { return d.deviceId == deviceId; });
    return target != devices.cend() && target->wake == u"ready"
           && std::any_of(devices.cbegin(), devices.cend(), [&deviceId](const rc::protocol::AccountDevice &d) {
                  return d.deviceId != deviceId && d.online && rc::server::acceptsConnections(d.platform);
              });
}

void DeviceWake::setAttempt(const QString &deviceId, const QString &state, const QString &note)
{
    const int serial = ++serial_;
    attempts_.insert(deviceId, {state, note, serial});
    // 发出去了等它上线：等够了忘掉这一次（又能点"唤醒"）
    if (state == Sent) {
        QTimer::singleShot(SentKeepMs, this, [this, deviceId, serial] {
            if (attempts_.value(deviceId).serial != serial)
                return;
            attempts_.remove(deviceId);
            ++revision_;
            emit changed();
        });
    }
    ++revision_;
    emit changed();
    if (state == Failed)
        emit failed(deviceId);
}

void DeviceWake::answered(const QString &deviceId, const QString &result, const QString &helper)
{
    if (attempts_.value(deviceId).state != Asking)
        return;
    qInfo("唤醒 %s：%s", qUtf8Printable(deviceId), qUtf8Printable(result));
    if (result == u"sent")
        setAttempt(deviceId, Sent, tr("已请“%1”帮忙发出唤醒包").arg(helper));
    else
        setAttempt(deviceId, Failed, failureNote(result));
}

// 账号下的设备上线、下线了：能不能请别的电脑帮忙跟着变；叫过的上线了就忘掉那一次
void DeviceWake::accountChanged()
{
    for (const rc::protocol::AccountDevice &device : account_.otherDevices()) {
        if (device.online)
            attempts_.remove(device.deviceId);
    }
    ++revision_;
    emit changed();
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
