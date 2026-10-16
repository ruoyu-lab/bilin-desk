#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <optional>

// 手机的窗口（QML 单例）：
// - keepAwake：连着电脑时屏幕不自己熄灭（设置里的"连接时屏幕常亮"，MobileSession 管）
// - dark：状态栏、导航条的图标跟着程序的外观（程序选了深色、手机是浅色时，图标要是白的），MobileMain 管
// - pressFeedback()：按住不放出了右键、开始拖时手机震一下
// - hideHomeIndicator()：全屏的平板，iPad 底部的横条不碰时隐去（TabletMain 管）
class PhoneWindow : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool keepAwake READ keepAwake WRITE setKeepAwake NOTIFY keepAwakeChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    using QObject::QObject;

    bool keepAwake() const { return keepAwake_; }
    void setKeepAwake(bool keep);
    bool dark() const { return dark_.value_or(false); }
    void setDark(bool dark);

    Q_INVOKABLE void pressFeedback();
    Q_INVOKABLE void hideHomeIndicator();

signals:
    void keepAwakeChanged();
    void darkChanged();

private:
    bool keepAwake_ = false;
    // 还没设过时不知道系统栏现在是什么样（打开时按手机的浅色、深色，platform/android 的 styles.xml）：第一次一定设
    std::optional<bool> dark_;
};
