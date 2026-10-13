#include "lan_discovery.h"

#include "device_store.h"

#include <rc/server_protocol.h>

#include <QNetworkInterface>

namespace protocol = rc::protocol;

LanDiscovery::LanDiscovery(DeviceStore &store, Options options, QObject *parent)
    : QObject(parent)
    , store_(store)
    , options_(options)
{
    timer_.setInterval(options_.announceMs);
    connect(&timer_, &QTimer::timeout, this, [this] {
        announce();
        forget();
    });
    connect(&socket_, &QUdpSocket::readyRead, this, &LanDiscovery::receive);
    connect(&store_, &DeviceStore::changed, this, &LanDiscovery::update);
    update();
}

LanDiscovery::~LanDiscovery()
{
    stop();
}

protocol::NearbyState LanDiscovery::state() const
{
    protocol::NearbyState state;
    for (const Device &device : devices_)
        state.devices.append(device.info);
    return state;
}

QString LanDiscovery::addressOf(const QString &deviceId) const
{
    const auto device = devices_.constFind(deviceId);
    return device == devices_.cend() ? QString() : device->info.address;
}

void LanDiscovery::update()
{
    if (!store_.state().lanDiscovery) {
        stop();
        return;
    }
    if (!running_)
        start();
    else
        announce(); // 名字变了时附近的马上看到
}

void LanDiscovery::start()
{
    // 同一台电脑上别的程序也可能在听这个端口（比如测试）
    if (!socket_.bind(QHostAddress::AnyIPv4, options_.port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning("发现附近的设备：端口 %d 用不了（%s）", options_.port, qUtf8Printable(socket_.errorString()));
        return;
    }
    running_ = true;
    qInfo("开始发现附近的设备");
    announce();
    timer_.start();
}

void LanDiscovery::stop()
{
    if (!running_)
        return;
    running_ = false;
    // 说过本机在的，走之前也说一声
    if (!announced_.deviceId.isEmpty())
        broadcast(rc::lan::leave(announced_.deviceId));
    announced_ = {};
    timer_.stop();
    socket_.close();
    qInfo("不再发现附近的设备");
    if (!devices_.isEmpty()) {
        devices_.clear();
        emit changed();
    }
}

void LanDiscovery::announce()
{
    if (!options_.announce)
        return;
    const auto device = store_.state();
    announced_ = {device.deviceId, device.deviceName, rc::server::platformName()};
    broadcast(rc::lan::announce(announced_));
}

// 往每块网卡的广播地址发（只发 255.255.255.255 时系统只从默认的那块网卡发出去）
void LanDiscovery::broadcast(const QByteArray &datagram)
{
    QList<QHostAddress> targets{QHostAddress::Broadcast};
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                targets.append(entry.broadcast());
        }
    }
    for (const QHostAddress &target : std::as_const(targets))
        socket_.writeDatagram(datagram, target, options_.port);
}

void LanDiscovery::receive()
{
    bool changed = false;
    while (socket_.hasPendingDatagrams()) {
        QByteArray datagram(qsizetype(socket_.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        socket_.readDatagram(datagram.data(), datagram.size(), &sender);
        const auto presence = rc::lan::parsePresence(datagram);
        if (!presence || presence->deviceId == announced_.deviceId)
            continue;
        if (presence->name.isEmpty()) {
            changed = devices_.remove(presence->deviceId) || changed;
            continue;
        }
        // 只要 IPv4 的样子，不要 ::ffff:a.b.c.d
        const protocol::NearbyDevice info{presence->deviceId, presence->name, presence->platform,
                                          QHostAddress(sender.toIPv4Address()).toString()};
        Device &device = devices_[presence->deviceId];
        changed = changed || device.info != info;
        device.info = info;
        device.expires = QDeadlineTimer(options_.forgetMs);
    }
    if (changed)
        emit this->changed();
}

void LanDiscovery::forget()
{
    if (devices_.removeIf([](const auto &entry) { return entry.value().expires.hasExpired(); }) > 0)
        emit changed();
}
