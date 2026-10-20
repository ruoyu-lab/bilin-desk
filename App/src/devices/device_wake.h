#pragma once

#include <rcsdk/power.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class Account;
class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 网络唤醒（QML 单例 Wake）：叫醒睡眠、关机了的我的设备（SDK 电源模块的 Wake-on-LAN，docs/devices-and-accounts.md）。
// 叫醒它要从它所在的网段里发"魔术包"：
// - 本机和它在同一个网络里：从本机发。连着我的设备时它告诉本机自己能唤醒的网卡（MAC 地址、IPv4 地址和网段；
//   share_wake_info，只给我的设备），记在本机（状态目录的 wake.json），它不在线时也用得上
// - 不然（或者本机发不出去，比如 iPad）：请账号下开着的别的电脑帮忙（远程唤醒）。它登录着账号时把网卡告诉了服务器，
//   服务器转给账号下开着的电脑，和它在同一个网段的那台发；结果经后台服务回来（wakeResult 事件）
class DeviceWake : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Wake)
    QML_SINGLETON

    // 叫不叫得醒、叫的结果变一次加一：界面按它重新问 problem()、state()、status()
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。file：记着的网卡存在这里
    DeviceWake(ServiceClient &service, Account &account, QString file, QObject *parent);

    static void setInstance(DeviceWake *instance);
    static DeviceWake *instance();
    static DeviceWake *create(QQmlEngine *, QJSEngine *);

    int revision() const { return revision_; }

    // 设备 deviceId（9 位数字）这次连上时说的网卡；说的和记着的不一样才存
    void remember(const QString &deviceId, const QList<rcs_power_interface> &interfaces);
    // 现在能不能叫它：能时为空（本机和它在同一个网络里，或者账号下有开着的别的电脑能帮忙），不然是为什么（给界面显示）。
    // deviceId 三位一组的也行（下同）
    Q_INVOKABLE QString problem(const QString &deviceId) const;
    // 叫醒它：本机能发就从本机发（发唤醒包，隔一秒一个，共三个：包可能丢）；不然请别的电脑帮忙，结果稍后来
    // （state()、status()，没成时 failed）。叫不了时 false
    Q_INVOKABLE bool wake(const QString &deviceId);
    // 上一次叫它到哪了：asking（在等别的电脑回答）| sent（唤醒包发出去了，等它上线）| failed | 空（没叫过、上线了、
    // 等它上线等够了）
    Q_INVOKABLE QString state(const QString &deviceId) const;
    // 给界面看的一句：叫过时说叫得怎么样，没叫过时说能怎么叫醒它、叫不醒为什么
    Q_INVOKABLE QString status(const QString &deviceId) const;

signals:
    void changed();
    // 请别的电脑帮忙叫醒它没成，或者本机发不出去又没人能帮忙（status() 说为什么）
    void failed(const QString &deviceId);

private:
    // 叫过一次：到哪了、给界面看的一句；serial 认出是哪一次（等它上线等够了就忘掉）
    struct Attempt
    {
        QString state;
        QString note;
        int serial = 0;
    };

    // 它的网卡里和本机在同一个网段的（发唤醒包的地址按它的网段）
    QList<rcs_power_interface> reachable(const QString &deviceId) const;
    // 能请别的电脑帮忙：它告诉过服务器网卡，账号下（除了本机和它）有开着的电脑
    bool helpable(const QString &deviceId) const;
    void setAttempt(const QString &deviceId, const QString &state, const QString &note);
    void answered(const QString &deviceId, const QString &result, const QString &helper);
    void accountChanged();
    void load();
    void save() const;

    ServiceClient &service_;
    Account &account_;
    QString file_;
    QHash<QString, QList<rcs_power_interface>> devices_;
    QHash<QString, Attempt> attempts_;
    int serial_ = 0;
    int revision_ = 0;
};
