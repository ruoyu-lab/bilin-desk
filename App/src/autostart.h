#pragma once

#include "login_item.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>

class QJSEngine;
class QQmlEngine;

// 开机时自动启动，给设置页和 Main.qml 用。开关就是系统里的登录项（login_item.h），改了就改系统里的；
// 用户在系统设置里改了，回到程序时跟着变。
// 装好了系统服务（后台服务一直在）时，开关还管界面程序没开着时它接不接受我的设备连接（followService）：
// 关了就像没有系统服务一样，只有打开程序时才能被连接。
// 默认开：第一次运行时替用户打开，只这一次，之后用户关了就不再打开。
// 登录时由系统启动的只在托盘 / 菜单栏，不弹窗口（launched）
class Autostart : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 登录时会自动启动
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    // 开关之外要说明的：approval（系统设置里没允许，要用户去那里打开）| refused（上一次没改成，
    // 被系统或安全软件拦下了）；没有时为空
    Q_PROPERTY(QString problem READ problem NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。要在 exec() 之前创建（见 launched）
    explicit Autostart(QObject *parent);

    static void setInstance(Autostart *instance);
    static Autostart *create(QQmlEngine *, QJSEngine *);

    bool enabled() const { return loginItemOn() && (!followed_ || followed_->get()); }
    void setEnabled(bool enabled);
    // 开关也管后台服务那边的一个设置：get 读它现在是什么，set 去改它（改好以后发 changed）
    void followService(std::function<bool()> get, std::function<void(bool)> set);
    QString problem() const;

    // 打开系统设置里管登录时启动的那一页
    Q_INVOKABLE void openSystemSettings();

signals:
    void changed();
    // 程序启动完了（事件循环开始以后）；atLogin：是登录时由系统启动的
    void launched(bool atLogin);

private:
    struct Followed
    {
        std::function<bool()> get;
        std::function<void(bool)> set;
    };

    bool loginItemOn() const { return status_ == login_item::Status::On; }
    void refresh();

    login_item::Status status_;
    std::optional<Followed> followed_;
    bool refused_ = false; // 上一次改没改成，又不是因为要用户批准
};
