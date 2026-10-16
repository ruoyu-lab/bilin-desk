#pragma once

#include <rcsdk/power.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// 网络唤醒（QML 单例 Wake）：叫醒睡眠、关机了的我的设备（SDK 电源模块的 Wake-on-LAN）。
// 连着我的设备时它告诉本机自己能唤醒的网卡（MAC 地址、IPv4 地址和网段；share_wake_info，只给我的设备），
// 记在本机（状态目录的 wake.json），它不在线时也用得上。叫醒它要从同一个网段里发"魔术包"：
// 现在只从本机发，本机和它不在同一个网络里时叫不醒（docs/devices-and-accounts.md）
class DeviceWake : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Wake)
    QML_SINGLETON

    // 记着的网卡变一次加一：界面按它重新问 problem()
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。file：记着的网卡存在这里
    DeviceWake(QString file, QObject *parent);

    static void setInstance(DeviceWake *instance);
    static DeviceWake *instance();
    static DeviceWake *create(QQmlEngine *, QJSEngine *);

    int revision() const { return revision_; }

    // 设备 deviceId（9 位数字）这次连上时说的网卡；说的和记着的不一样才存
    void remember(const QString &deviceId, const QList<rcs_power_interface> &interfaces);
    // 现在叫不叫得醒它：叫得醒时为空，不然是为什么（给界面显示）。deviceId 三位一组的也行
    Q_INVOKABLE QString problem(const QString &deviceId) const;
    // 发唤醒包（隔一秒一个，共三个：包可能丢）；叫不醒时 false
    Q_INVOKABLE bool wake(const QString &deviceId);

signals:
    // 记着的网卡变了：problem() 可能跟着变
    void changed();

private:
    // 它的网卡里和本机在同一个网段的（发唤醒包的地址按它的网段）
    QList<rcs_power_interface> reachable(const QString &deviceId) const;
    void load();
    void save() const;

    QString file_;
    QHash<QString, QList<rcs_power_interface>> devices_;
    QHash<QString, QString> unsent_; // 上次叫它时唤醒包为什么没发出去（problem() 说）
    int revision_ = 0;
};
