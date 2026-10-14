#include "session_keeper.h"

#include "session/viewer_sessions.h"

#include <QFuture>
#include <QGuiApplication>
#include <QJniObject>
#include <QSettings>
#include <QtCore/private/qandroidextras_p.h>

namespace {

constexpr char ServiceClass[] = "com/bilindesk/bilin/SessionService";
constexpr auto NotificationPermission = QLatin1StringView("android.permission.POST_NOTIFICATIONS");
// 问过能不能发通知了（只问一次，不管给没给）
constexpr auto NotificationsAskedKey = QLatin1StringView("mobile/notificationsAsked");

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
    if (wanted == serviceRunning_ || asking_)
        return;
    // 先问完再开前台服务：开的时候没允许，之后允许了通知也不会出来
    if (wanted && shouldAskForNotifications()) {
        asking_ = true;
        QSettings().setValue(NotificationsAskedKey, true);
        QtAndroidPrivate::requestPermission(NotificationPermission).then(this, [this](QtAndroidPrivate::PermissionResult) {
            asking_ = false;
            update();
        });
        return;
    }
    serviceRunning_ = wanted;
    callService(wanted ? "start" : "stop");
}

bool SessionKeeper::shouldAskForNotifications() const
{
    return QNativeInterface::QAndroidApplication::sdkVersion() >= 33 && !QSettings().value(NotificationsAskedKey).toBool()
           && QtAndroidPrivate::checkPermission(NotificationPermission).result() != QtAndroidPrivate::Authorized;
}
