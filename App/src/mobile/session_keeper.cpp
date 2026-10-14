#include "session_keeper.h"

#include "session/viewer_sessions.h"

#include <QGuiApplication>
#include <QJniObject>

namespace {

constexpr char ServiceClass[] = "com/bilindesk/bilin/SessionService";

void callService(const char *method)
{
    QJniObject::callStaticMethod<void>(ServiceClass, method, "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context().object());
}

} // namespace

SessionKeeper::SessionKeeper(ViewerSessions &sessions, QObject *parent)
    : QObject(parent)
    , sessions_(sessions)
{
    connect(&sessions_, &ViewerSessions::sessionsChanged, this, &SessionKeeper::update);
    // 不在前面：别的界面盖住了（系统的文件选择、最近任务）、切走了、锁屏了。分屏时不算（Qt 不报）
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) { sessions_.setHidden(state != Qt::ApplicationActive); });
}

void SessionKeeper::update()
{
    const bool wanted = !sessions_.isEmpty();
    if (wanted == serviceRunning_)
        return;
    serviceRunning_ = wanted;
    callService(wanted ? "start" : "stop");
}
