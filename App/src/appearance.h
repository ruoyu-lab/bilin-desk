#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// 外观（设置里）：跟随系统 | 浅色 | 深色，记在偏好设置里（colorScheme）。程序一启动就设给系统（QStyleHints）：
// 窗口、系统画的标题栏和控件跟着它；界面的颜色由 Theme.qml 按 dark 换
class Appearance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // system | light | dark
    Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY changed)
    // 现在用的是深色（跟随系统时看系统的）
    Q_PROPERTY(bool dark READ dark NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个
    explicit Appearance(QObject *parent);

    static void setInstance(Appearance *instance);
    static Appearance *create(QQmlEngine *, QJSEngine *);

    QString scheme() const { return scheme_; }
    void setScheme(const QString &scheme);
    bool dark() const;

signals:
    void changed();

private:
    void apply();

    QString scheme_;
};
