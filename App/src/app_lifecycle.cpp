#include "app_lifecycle.h"

#include "dock.h"
#include "quit_reason.h"

#include <rc/programs.h>

#include <QCoreApplication>
#include <QEvent>
#include <QJSEngine>

namespace {

AppLifecycle *instance = nullptr;

} // namespace

AppLifecycle::AppLifecycle(QObject *parent)
    : QObject(parent)
    , background_(QCoreApplication::arguments().contains(rc::program::BackgroundArgument))
{
    // 系统要求退出（macOS 的 Cmd+Q、注销）时先收到 QEvent::Quit，之后才逐个关窗口
    QCoreApplication::instance()->installEventFilter(this);
}

bool AppLifecycle::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == QCoreApplication::instance() && event->type() == QEvent::Quit && !quitting_) {
        // 不接受：系统取消这次退出（macOS 上 Qt 回答 AppKit 的 applicationShouldTerminate）
        if (holdQuit_ && quitAskedByUser()) {
            event->ignore();
            emit quitHeld();
            return true;
        }
        quitting_ = true;
        emit quittingChanged();
    }
    return QObject::eventFilter(watched, event);
}

void AppLifecycle::setInstance(AppLifecycle *lifecycle)
{
    instance = lifecycle;
}

AppLifecycle *AppLifecycle::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void AppLifecycle::quit()
{
    if (!quitting_) {
        quitting_ = true;
        emit quittingChanged();
    }
    QCoreApplication::quit();
}

void AppLifecycle::setDockVisible(bool visible)
{
    setDockIconVisible(visible);
}
