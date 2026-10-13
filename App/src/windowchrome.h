#pragma once

#include <QObject>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// 平台窗口外观。
// macOS 上通过一个空的 NSToolbar 控制标题栏样式：
//   "unified" — 标准高度：大圆角，红绿灯与 52px 工具栏同一行
//   "compact" — 紧凑高度：红绿灯所在行更矮
// Windows 上窗口外框交给 QWindowKit：系统边框、阴影、圆角和贴靠布局保留，系统标题栏去掉，
// 标题栏和最小化 / 最大化 / 关闭由程序自己画，再登记给它（setTitleBar、setWindowButton、setInteractive）。
// 不适用的平台上这些调用不做任何事。
class WindowChrome : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE void applyTitleBar(QWindow *window, const QString &style);
    // 隐藏或恢复空工具栏：全屏时需要隐藏，否则顶部会留一条空白工具栏
    Q_INVOKABLE void setToolbarVisible(QWindow *window, bool visible);
    // 红绿灯在窗口坐标系中的外接矩形；无此类按钮的平台返回空矩形
    Q_INVOKABLE QRectF titleButtonsRect(QWindow *window) const;
    // point（Qt 的全局坐标，逻辑像素）所在屏幕上摄像头刘海的矩形；没有刘海的屏幕和平台返回空矩形
    Q_INVOKABLE QRectF screenNotch(QPointF point) const;
    // point 所在屏幕顶部菜单栏的高度（逻辑像素）。菜单栏自动隐藏、这块屏幕上没有菜单栏时
    // 是系统菜单栏的标准高度；没有菜单栏的平台返回 0
    Q_INVOKABLE qreal menuBarHeight(QPointF point) const;
    // macOS：窗口放到菜单栏那一层、上沿贴住屏幕顶端，所有桌面空间和全屏应用上都显示。
    // 窗口显示出来以后调用（系统把窗口摆上屏幕时会让它避开菜单栏）
    Q_INVOKABLE void placeOverMenuBar(QWindow *window);

    // Windows：item 所在的区域是标题栏，按住拖动窗口、双击最大化
    Q_INVOKABLE void setTitleBar(QQuickItem *item);
    // Windows：item 是窗口按钮，role 为 "minimize"、"maximize" 或 "close"。
    // 登记了最大化按钮，Windows 11 上悬停时弹出贴靠布局
    Q_INVOKABLE void setWindowButton(QQuickItem *item, const QString &role);
    // Windows：标题栏里可以点的东西（工具按钮、标签、搜索框），不当作拖动区域
    Q_INVOKABLE void setInteractive(QQuickItem *item);
};
