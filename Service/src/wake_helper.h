#pragma once

#include <rc/server_protocol.h>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QHash>
#include <QLatin1StringView>
#include <QList>
#include <QObject>

// 远程唤醒里本机的那一份（docs/devices-and-accounts.md 的"远程唤醒"）：
// - 本机能唤醒它的网卡（SDK 的 rcs_power_list_interfaces），ServerLink 登录着账号时告诉服务器
// - 服务器请本机叫醒账号下睡着的一台：它的网卡和本机连着的网卡在同一个网段时，从本机发唤醒包。发到本机那块网卡的
//   广播地址，由本机自己算，不用服务器给的地址：唤醒包只会出现在本机所在的局域网里。同一块网卡 MinGapMs 里只发一轮
class WakeHelper : public QObject
{
    Q_OBJECT

public:
    // 一轮三个包，隔一秒（包可能丢）；同一块网卡两轮之间至少隔这么久
    static constexpr int Packets = 3;
    static constexpr int PacketGapMs = 1000;
    static constexpr int MinGapMs = 10000;

    explicit WakeHelper(QObject *parent = nullptr);

    // 列本机能唤醒它的网卡（要一会儿：在别的线程上列），列好了 interfacesListed
    void listInterfaces();
    // 叫醒它（targets：它能唤醒自己的网卡）：rc::server 的 WakeSent | WakeElsewhere（本机和它不在同一个网段）
    // | WakeFailed（系统不让发）
    QLatin1StringView wake(const QList<rc::server::WakeInterface> &targets);

signals:
    // 列不出来（系统不支持、出错了）时是空的
    void interfacesListed(const QList<rc::server::WakeInterface> &interfaces);

private:
    QHash<QByteArray, QDeadlineTimer> recent_; // MAC 地址 → 这之前不再发
};
