#pragma once

#include <rc/lan_protocol.h>
#include <rc/protocol.h>

#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

class DeviceStore;

// 局域网里附近的设备（rc/lan_protocol.h 的发现）：开着"发现附近的设备"时隔一会儿广播一次本机，
// 也记下听到的别人；关着时不发也不听。同一网络里装了本程序、开着它的都能看到本机的名字和设备 ID
class LanDiscovery : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        quint16 port = rc::lan::DiscoveryPort;
        int announceMs = rc::lan::AnnounceMs;
        int forgetMs = rc::lan::ForgetMs;
    };

    LanDiscovery(DeviceStore &store, Options options, QObject *parent = nullptr);
    // 走之前告诉附近的设备
    ~LanDiscovery() override;

    rc::protocol::NearbyState state() const;
    // 附近这台设备的地址；不在附近时为空
    QString addressOf(const QString &deviceId) const;

signals:
    void changed();

private:
    struct Device
    {
        rc::protocol::NearbyDevice info;
        QDeadlineTimer expires;
    };

    void update();
    void start();
    void stop();
    void announce();
    void broadcast(const QByteArray &datagram);
    void receive();
    void forget();

    DeviceStore &store_;
    const Options options_;
    QUdpSocket socket_;
    QTimer timer_;
    QHash<QString, Device> devices_; // 按设备 ID
    rc::lan::Presence announced_;
    bool running_ = false;
};
