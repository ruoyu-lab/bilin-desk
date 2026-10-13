#pragma once

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// 鼠标在屏幕上的位置（Qt 的全局坐标，逻辑像素），active 时定时读一次。
// 用来判断鼠标在不在某块区域上：窗口自己的悬停事件靠不住。macOS 上程序不在前台时，
// Qt 丢掉鼠标移进移出的事件；几个窗口叠着出现时，进出的先后也不一定
class CursorPosition : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)

public:
    explicit CursorPosition(QObject *parent = nullptr);

    bool isActive() const { return timer_.isActive(); }
    void setActive(bool active);
    QPointF position() const { return position_; }

signals:
    void activeChanged();
    void positionChanged();

private:
    void poll();

    QTimer timer_;
    QPointF position_;
};
