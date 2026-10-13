#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// 程序的生命周期给 QML 用的一面。窗口按需创建、关掉就销毁，关掉窗口不退出程序：
// 托盘 / 菜单栏图标一直在。
// - showRequested：再次启动程序、点 Dock 图标（macOS）时，把窗口拿出来
// - 有人连着本机时（holdQuit）用户要退出（macOS 的 Cmd+Q、Dock 里的"退出"）：先不退，quitHeld 让界面问一句；
//   注销、关机不拦。托盘的"退出"在 QML 里先问过了
class AppLifecycle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 程序正在退出（托盘的"退出"、Cmd+Q、系统注销）：窗口这时不拦关闭，否则退出会被取消
    Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)
    // 后台服务把界面程序拉起来的（有人连着本机时界面没了，rc/programs.h 的 BackgroundArgument）：不弹窗口
    Q_PROPERTY(bool background READ background CONSTANT)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个，而不是自己再建一个
    explicit AppLifecycle(QObject *parent);

    static void setInstance(AppLifecycle *instance);
    static AppLifecycle *create(QQmlEngine *, QJSEngine *);

    bool quitting() const { return quitting_; }
    bool background() const { return background_; }
    // 有人连着本机：用户要退出时先问（main 里跟着 IncomingConnections 设）
    void setHoldQuit(bool hold) { holdQuit_ = hold; }
    Q_INVOKABLE void quit();
    // macOS：有窗口时 Dock 里有图标，没有窗口时只留菜单栏的图标
    Q_INVOKABLE void setDockVisible(bool visible);

signals:
    void showRequested();
    void quittingChanged();
    // 用户要退出，但有人连着本机：问一句，确认了再 quit()
    void quitHeld();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    const bool background_;
    bool quitting_ = false;
    bool holdQuit_ = false;
};
