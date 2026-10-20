#include "wake_helper.h"

#include <rcsdk/power.h>

#include <QHostAddress>
#include <QNetworkInterface>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace {

using rc::server::WakeInterface;

QList<WakeInterface> localInterfaces()
{
    std::array<rcs_power_interface, RCS_POWER_INTERFACES_MAX> items{};
    uint32_t count = 0;
    if (const rcs_result result = rcs_power_list_interfaces(items.data(), sizeof(rcs_power_interface),
                                                            uint32_t(items.size()), &count);
        result != RCS_OK) {
        qWarning("列不出本机能唤醒它的网卡：%s", rcs_result_message(result));
        return {};
    }
    QList<WakeInterface> interfaces;
    for (uint32_t i = 0; i < std::min<uint32_t>(count, uint32_t(items.size())); ++i) {
        const rcs_power_interface &item = items[i];
        // 没有 IPv4 地址的：别的电脑看不出和它在不在同一个网段
        if (item.address[0] == '\0')
            continue;
        WakeInterface known;
        known.mac = QByteArray(reinterpret_cast<const char *>(item.mac), 6);
        known.address = QString::fromLatin1(item.address);
        known.prefix = int(item.prefix_length);
        known.wifi = item.link == RCS_POWER_LINK_WIFI;
        known.wake = item.wake == RCS_POWER_WAKE_ENABLED    ? WakeInterface::Setting::Enabled
                     : item.wake == RCS_POWER_WAKE_DISABLED ? WakeInterface::Setting::Disabled
                                                            : WakeInterface::Setting::Unknown;
        interfaces.append(known);
    }
    return interfaces;
}

// 往它的一块网卡发一个唤醒包，经本机在那个网段的广播地址
struct Shot
{
    QByteArray mac;
    QByteArray broadcast;

    friend bool operator==(const Shot &, const Shot &) = default;
};

rcs_result send(const Shot &shot)
{
    rcs_power_wake wake = rcs_power_wake_init();
    std::memcpy(wake.mac, shot.mac.constData(), 6);
    wake.address = shot.broadcast.constData();
    const rcs_result result = rcs_power_send_wake(&wake);
    if (result != RCS_OK)
        qWarning("唤醒包没发出去（%s）：%s", shot.mac.toHex(':').constData(), rcs_result_message(result));
    return result;
}

} // namespace

WakeHelper::WakeHelper(QObject *parent)
    : QObject(parent)
{
}

void WakeHelper::listInterfaces()
{
    const auto interfaces = std::make_shared<QList<WakeInterface>>();
    QThread *thread = QThread::create([interfaces] { *interfaces = localInterfaces(); });
    connect(thread, &QThread::finished, this, [this, interfaces] { emit interfacesListed(*interfaces); });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QLatin1StringView WakeHelper::wake(const QList<WakeInterface> &targets)
{
    // 本机连着的网卡里，和它的网卡在同一个网段的那块
    QList<Shot> shots;
    for (const QNetworkInterface &network : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = network.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : network.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || entry.broadcast().isNull())
                continue;
            for (const WakeInterface &target : targets) {
                const Shot shot{target.mac, entry.broadcast().toString().toLatin1()};
                // 本机有两块网卡在同一个网段（网线、Wi-Fi 都连着）：发一次就够了
                if (target.wake != WakeInterface::Setting::Disabled
                    && QHostAddress(target.address).isInSubnet(entry.ip(), entry.prefixLength())
                    && !shots.contains(shot))
                    shots.append(shot);
            }
        }
    }
    if (shots.isEmpty())
        return rc::server::WakeElsewhere;
    // 刚发过一轮的网卡这次不再发，算发了
    shots.removeIf([this](const Shot &shot) {
        const auto found = recent_.constFind(shot.mac);
        return found != recent_.cend() && !found->hasExpired();
    });
    if (shots.isEmpty())
        return rc::server::WakeSent;

    qInfo("帮忙叫醒账号下的设备：往 %lld 块网卡发唤醒包", qlonglong(shots.size()));
    bool sent = false;
    for (const Shot &shot : shots)
        sent = send(shot) == RCS_OK || sent;
    if (!sent)
        return rc::server::WakeFailed;
    for (const Shot &shot : shots)
        recent_.insert(shot.mac, QDeadlineTimer(MinGapMs));
    for (int packet = 1; packet < Packets; ++packet) {
        QTimer::singleShot(packet * PacketGapMs, this, [shots] {
            for (const Shot &shot : shots)
                send(shot);
        });
    }
    return rc::server::WakeSent;
}
