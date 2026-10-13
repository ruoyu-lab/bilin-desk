#pragma once

#include <rc/protocol.h>

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 本机给界面用的一面：设备 ID、名字、临时密码和被控那一面的设置，都在后台服务里（rc/protocol.h 的 device 主题）。
// 改设置是给后台服务发命令，它改好以后发回来的新状态才算数
class ThisDevice : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // connecting：正在连后台服务（第一次要先启动它）| ready | unavailable：后台服务起不来
    Q_PROPERTY(QString serviceState READ serviceState NOTIFY serviceStateChanged)
    // 三位一组；还没连上过后台服务时是空的
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY changed)
    Q_PROPERTY(QString deviceName READ deviceName WRITE setDeviceName NOTIFY changed)
    Q_PROPERTY(QString password READ password NOTIFY changed)
    Q_PROPERTY(bool acceptIncoming READ acceptIncoming WRITE setAcceptIncoming NOTIFY changed)
    Q_PROPERTY(bool lanDiscovery READ lanDiscovery WRITE setLanDiscovery NOTIFY changed)
    // 界面程序没开着时也接受我的设备连接（只在后台服务是装好的系统服务时有意义，见 systemService）
    Q_PROPERTY(bool unattended READ unattended WRITE setUnattended NOTIFY changed)
    // 程序的新版本（rc/protocol.h 的 UpdateState，只有装好的程序有）：服务器上比本机新的版本，没有时为空
    Q_PROPERTY(QString updateAvailable READ updateAvailable NOTIFY updateChanged)
    // idle | downloading | installing
    Q_PROPERTY(QString updatePhase READ updatePhase NOTIFY updateChanged)
    // 下载了多少（0~1）
    Q_PROPERTY(double updateProgress READ updateProgress NOTIFY updateChanged)
    // 上一次没成的那一步：download | install；没有时为空
    Q_PROPERTY(QString updateFailed READ updateFailed NOTIFY updateChanged)
    // 点了检查更新的结果：upToDate | failed；没点过、有新版本时为空（检查的时候 updatePhase 是 checking）
    Q_PROPERTY(QString updateChecked READ updateChecked NOTIFY updateChanged)
    // 后台服务是装好的系统服务：才有新版本、能检查更新
    Q_PROPERTY(bool systemService READ systemService CONSTANT)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个
    ThisDevice(ServiceClient &service, QObject *parent);

    static void setInstance(ThisDevice *instance);
    static ThisDevice *create(QQmlEngine *, QJSEngine *);

    QString serviceState() const;
    QString deviceId() const;
    QString deviceName() const { return state_.deviceName; }
    // 空的：用回本机默认的名字
    void setDeviceName(const QString &name);
    QString password() const { return state_.password; }
    bool acceptIncoming() const { return state_.acceptIncoming; }
    void setAcceptIncoming(bool accept);
    bool lanDiscovery() const { return state_.lanDiscovery; }
    void setLanDiscovery(bool enabled);
    bool unattended() const { return state_.unattended; }
    void setUnattended(bool unattended);
    // 后台服务是装好的系统服务（rc/system_service.h）
    bool systemService() const;

    QString updateAvailable() const { return update_.available; }
    QString updatePhase() const { return update_.phase; }
    double updateProgress() const { return update_.progress; }
    QString updateFailed() const { return update_.failed; }
    QString updateChecked() const { return update_.checked; }

    Q_INVOKABLE void regeneratePassword();
    // 下载、安装新版本：装的时候本程序会被关掉，装完再打开
    Q_INVOKABLE void installUpdate();
    // 马上问服务器有没有新版本
    Q_INVOKABLE void checkUpdate();
    Q_INVOKABLE void retry();
    // 完全退出：后台服务也退出，本机不再能被连接
    Q_INVOKABLE void quitService();

signals:
    void serviceStateChanged();
    void changed();
    void updateChanged();

private:
    ServiceClient &service_;
    rc::protocol::DeviceState state_;
    rc::protocol::UpdateState update_;
};
