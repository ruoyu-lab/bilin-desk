#include "cursor_position.h"

#include <QCursor>

CursorPosition::CursorPosition(QObject *parent)
    : QObject(parent)
{
    // 二十分之一秒：跟得上手，又不占什么
    timer_.setInterval(50);
    connect(&timer_, &QTimer::timeout, this, &CursorPosition::poll);
}

void CursorPosition::setActive(bool active)
{
    if (active == isActive())
        return;
    if (active) {
        poll();
        timer_.start();
    } else {
        timer_.stop();
    }
    emit activeChanged();
}

void CursorPosition::poll()
{
    const QPointF position = QCursor::pos();
    if (position == position_)
        return;
    position_ = position;
    emit positionChanged();
}
