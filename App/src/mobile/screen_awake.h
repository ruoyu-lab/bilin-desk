#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// 手机：连着电脑时屏幕不自己熄灭（设置里的"连接时屏幕常亮"，MobileSession 管）。
// Android 是窗口的 FLAG_KEEP_SCREEN_ON
class ScreenAwake : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool keep READ keep WRITE setKeep NOTIFY keepChanged)

public:
    using QObject::QObject;

    bool keep() const { return keep_; }
    void setKeep(bool keep);

signals:
    void keepChanged();

private:
    bool keep_ = false;
};
