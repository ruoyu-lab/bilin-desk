#include "windowchrome.h"

#ifdef Q_OS_WIN
#include <QWKQuick/quickwindowagent.h>
#include <qt_windows.h>

namespace {

// 每个窗口一个 QWindowKit 代理，第一次用到时建立
QWK::QuickWindowAgent *agentFor(QQuickItem *item)
{
    QQuickWindow *window = item ? item->window() : nullptr;
    if (!window)
        return nullptr;
    if (auto *agent = window->findChild<QWK::QuickWindowAgent *>(Qt::FindDirectChildrenOnly))
        return agent;
    auto *agent = new QWK::QuickWindowAgent(window);
    agent->setup(window);
    return agent;
}

} // namespace
#endif

void WindowChrome::applyTitleBar(QWindow *, const QString &)
{
}

void WindowChrome::setToolbarVisible(QWindow *, bool)
{
}

QRectF WindowChrome::titleButtonsRect(QWindow *) const
{
    return {};
}

QRectF WindowChrome::screenNotch(QPointF) const
{
    return {};
}

qreal WindowChrome::menuBarHeight(QPointF) const
{
    return 0;
}

void WindowChrome::placeOverMenuBar(QWindow *)
{
}


void WindowChrome::setTitleBar(QQuickItem *item)
{
#ifdef Q_OS_WIN
    if (auto *agent = agentFor(item))
        agent->setTitleBar(item);
#else
    Q_UNUSED(item);
#endif
}

void WindowChrome::setWindowButton(QQuickItem *item, const QString &role)
{
#ifdef Q_OS_WIN
    auto *agent = agentFor(item);
    if (!agent)
        return;
    if (role == u"minimize")
        agent->setSystemButton(QWK::WindowAgentBase::Minimize, item);
    else if (role == u"maximize")
        agent->setSystemButton(QWK::WindowAgentBase::Maximize, item);
    else if (role == u"close")
        agent->setSystemButton(QWK::WindowAgentBase::Close, item);
#else
    Q_UNUSED(item);
    Q_UNUSED(role);
#endif
}

void WindowChrome::setInteractive(QQuickItem *item)
{
#ifdef Q_OS_WIN
    if (auto *agent = agentFor(item))
        agent->setHitTestVisible(item, true);
#else
    Q_UNUSED(item);
#endif
}
