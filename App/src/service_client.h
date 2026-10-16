#pragma once

#include <QHash>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QObject>
#include <QString>
#include <QTimer>

namespace rc {
class LineChannel;
}

// 和后台服务之间的本机连接（消息见 rc/protocol.h）。后台服务由谁运行（Runner）：
// - Beside（电脑上的开发版）：连当前用户的后台服务，连不上时把它启动起来（它和界面程序放在一起），断了就重新连；
//   连上的不是旁边这一份程序（重新编译过、别处的一份）时让它退出，换成旁边这一份
// - System（电脑上装好的，rc/system_service.h）：连整台电脑那一个，它由系统启动、管着，这边只管连，断了就重新连
// - InProcess（手机）：在本程序的进程里（App/src/mobile/local_service.h），连上它之前已经在听
class ServiceClient : public QObject
{
    Q_OBJECT

public:
    // Connecting：正在连（需要时先启动它）| Ready | Unavailable：启动了也连不上
    enum class State { Connecting, Ready, Unavailable };

    enum class Runner { Beside, System, InProcess };

    // endpoint：后台服务的本机连接（rc/ipc/endpoint.h、rc/system_service.h）
    ServiceClient(Runner runner, const QString &endpoint, QObject *parent = nullptr);

    // 后台服务是装好的系统服务
    bool systemService() const { return runner_ == Runner::System; }

    void start();
    // 后台服务起不来时再试一次
    void retry();
    // 托盘里的"退出"：开发版后台服务也退出，之后不再重连；系统服务不退（我的设备照样能连）
    void stopService();
    // 界面程序要退出了（AppLifecycle）：后台服务断开连着本机的人，之后不把界面拉起来
    void appQuitting();

    State state() const { return state_; }
    QJsonObject topic(QLatin1StringView name) const { return topics_.value(name); }
    // 后台服务发过这个主题没有（刚连上时它一个一个发过来）
    bool hasTopic(QLatin1StringView name) const { return topics_.contains(name); }
    void send(QLatin1StringView command, const QJsonObject &arguments = {});

signals:
    void stateChanged();
    void topicChanged(const QString &name);
    // 后台服务的一次性通知（rc/protocol.h 的事件）
    void eventReceived(const QString &name, const QJsonObject &arguments);

private:
    void connectToService();
    void connectionFailed();
    void disconnected();
    void received(const QJsonObject &message);
    void checkService(const QJsonObject &info);
    void setState(State state);
    void launch();

    const Runner runner_;
    const QString endpoint_;
    rc::LineChannel *channel_ = nullptr;
    QHash<QString, QJsonObject> topics_;
    State state_ = State::Connecting;
    QTimer retryTimer_;
    int attempts_ = 0;      // 这一轮连了几次
    bool launched_ = false; // 这一轮启动过后台服务
    bool replaced_ = false; // 这一轮换过一次后台服务
    bool stopping_ = false;
};
