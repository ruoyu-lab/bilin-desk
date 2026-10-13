#pragma once

#include <rcsdk/power.h>

#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <optional>

namespace rcsdk {
class Session;
class SessionConfig;
}

// 去连我的设备时让它重启、关机、睡眠、注销（SDK 的电源模块）。对方提供哪几样看 RCS_POWER_PEER（它的系统
// 做得到的；别人的电脑一样都不提供）。界面上确认过就交给 SDK，对方答应以后 1 秒就做。
// 能不强制就不强制（程序会问要不要保存，在画面上就能处理）；对方做不了不强制的（Windows 以系统身份注销、
// Mac 的登录界面上）再强制一次。request() 是给界面的进展 {action, phase, message}，phase：
//   asking 等对方回答 | accepted 对方答应了，马上就做 | failed 没做成（message 说为什么）
// 对方答应以后（leaving()）连接很快会断：接下来怎么办由 ViewerSession 按做的是什么定
class ViewerPower : public QObject
{
    Q_OBJECT

public:
    // 电源模块的一个事件里要的部分：在 SDK 的线程上复制出来，再交给界面线程
    struct Event
    {
        rcs_module_event_kind kind = 0;
        uint32_t peerActions = 0; // RCS_POWER_PEER
        bool wakeInfo = false;    // RCS_POWER_PEER：对方说了它的网卡（rcs_power_get_peer_interfaces）
        rcs_power_action_event action{}; // RCS_POWER_ACTION
    };
    // 不是电源模块的事件时为空
    static std::optional<Event> read(const rcs_event &event);

    explicit ViewerPower(QObject *parent = nullptr);

    // 对方提供的："restart" | "shutDown" | "sleep" | "signOut"
    QStringList offered() const { return offered_; }
    QVariantMap request() const;
    // 对方答应要做的（连接接下来会断）；没有时为空
    QString leaving() const { return leaving_; }

    // 新的一次连接：用电源模块（本机不因为看着对方就不睡），进展从头来
    void configure(rcsdk::SessionConfig &config);
    void ask(const QString &action, rcsdk::Session &session);
    void handle(const Event &event, rcsdk::Session &session);
    // 这次连接结束了（或者对方做完、又连上了）：都不算了
    void reset();

signals:
    void changed();
    // 对方告诉了它能唤醒的网卡（只有我的设备会说）
    void wakeInterfaces(const QList<rcs_power_interface> &interfaces);

private:
    void send(rcsdk::Session &session, bool force);
    void setPhase(const QString &phase, const QString &message = {});

    QStringList offered_;
    QString action_;
    bool forced_ = false;
    QString phase_;
    QString message_;
    QString leaving_;
    // 没做成的，过一会儿不再显示
    QTimer clearTimer_;
};
