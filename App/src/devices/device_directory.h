#pragma once

#include <rc/protocol.h>

#include <QList>
#include <QObject>
#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Account;
class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 设备列表（QML 单例 Devices）：界面上能去连的设备合在一起——
// - 我的设备：登录的账号下别的设备（Account），知道在不在线
// - 附近的：同一网络里开着"发现附近的设备"的（后台服务的 nearby 主题），在线
// - 本机记着的（状态目录的 devices.json）：自己添加的（saved）、收藏的、连过的（lastSeen）
// 别人的设备看不到在不在线（docs/devices-and-accounts.md）：不在附近时只知道上次连接的时间。
//
// 每一项是 { key, deviceId（三位一组，按 IP 地址添加的为空）, ip（知道时）, address（名字下面显示的：
// IP 或设备 ID）, target（去连时给的：设备 ID 或 IP）, name, os, online, presenceKnown（知不知道在不在线）,
// mine（我的设备）, favorite, saved, discovered（在附近）, lastSeen（毫秒，0：没连过）, paired,
// alias（用户起的名字，没起过为空）, ownName（它自己的名字）, note（备注）, thumbnail（上次断开时画面的缩略图，没有为空）}
class DeviceDirectory : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Devices)
    QML_SINGLETON

    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。file：记着的设备存在这里
    DeviceDirectory(ServiceClient &service, Account &account, QString file, QObject *parent);

    static void setInstance(DeviceDirectory *instance);
    static DeviceDirectory *create(QQmlEngine *, QJSEngine *);

    QVariantList devices() const;

    // key 那一台；没有时为空
    Q_INVOKABLE QVariantMap find(const QString &key) const;
    // 用户输入的设备 ID 或 IP 地址（alias 是起的名字，可以为空）：已有的那一台，或者新的一台
    // （先不保存，连过以后在"最近"里）；都不是时为空
    Q_INVOKABLE QVariantMap ensure(const QString &text, const QString &alias);
    Q_INVOKABLE void toggleFavorite(const QString &key);
    // 加进"已保存"、从里面拿掉（收藏、连接记录不变）
    Q_INVOKABLE void save(const QString &key);
    Q_INVOKABLE void forget(const QString &key);
    // 刚去连了它
    Q_INVOKABLE void touch(const QString &key);
    // 用户起的名字（空：用回它自己的名字）、备注
    Q_INVOKABLE void rename(const QString &key, const QString &alias);
    Q_INVOKABLE void setNote(const QString &key, const QString &note);
    // 断开、切走时：把窗口里 picture（远程画面）那一块截下来，缩小存着，详情页顶上显示
    Q_INVOKABLE void saveThumbnail(const QString &key, QQuickItem *picture);

signals:
    void changed();

private:
    // 本机记着的一台
    struct Entry
    {
        QString deviceId; // 9 位数字；按 IP 地址添加的为空
        QString address;  // 按 IP 地址添加的
        QString alias;    // 用户起的名字
        QString name;     // 上次知道的它自己的名字
        QString note;     // 用户写的备注
        QString os;
        bool favorite = false;
        bool saved = false;
        qint64 lastSeen = 0;

        QString key() const;
        // 什么都没记的：不用存
        bool empty() const { return !favorite && !saved && lastSeen == 0 && alias.isEmpty() && note.isEmpty(); }
    };

    void nearbyChanged();
    void accountChanged();
    Entry *entry(const QString &key);
    // key 那一台的记录；是附近的、我的设备、还没记着时先建一个（还不存）；都不是时为空
    Entry *entryFor(const QString &key);
    const rc::protocol::NearbyDevice *nearby(const QString &deviceId) const;
    const rc::protocol::AccountDevice *mine(const QString &deviceId) const;
    QVariantMap toVariant(const Entry &entry) const;
    // key 那一台的缩略图存在哪
    QString thumbnailFile(const QString &key) const;
    void load();
    void store();

    ServiceClient &service_;
    Account &account_;
    const QString file_;
    QList<Entry> entries_;
    rc::protocol::NearbyState nearby_;
    QList<rc::protocol::AccountDevice> mine_;
};
