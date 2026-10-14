#pragma once

#include <QObject>

class ViewerSessions;

// Android 上切到别的程序、锁屏时连接不断：
// - 有会话时开着前台服务（platform/android 的 SessionService，通知栏里说正在连接）：系统不冻结本进程，SDK 自己的
//   线程照样维持连接（Qt 的事件循环在后台是停着的）。Android 13 起发通知要用户允许：第一次连接时问一次，不给也行
//   （通知栏里看不到，连接照样不断）
// - 程序不在前面时会话暂停画面（ViewerSessions::setHidden）：不在后台白收画面、白解码；回来时从关键帧接着
class SessionKeeper : public QObject
{
    Q_OBJECT

public:
    SessionKeeper(ViewerSessions &sessions, QObject *parent = nullptr);

private:
    void update();
    // 这次要不要先问能不能发通知：Android 13 起、还没问过、还没允许
    bool shouldAskForNotifications() const;

    ViewerSessions &sessions_;
    bool serviceRunning_ = false;
    bool asking_ = false;
};
