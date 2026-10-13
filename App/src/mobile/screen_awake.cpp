#include "screen_awake.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

void ScreenAwake::setKeep(bool keep)
{
    if (keep == keep_)
        return;
    keep_ = keep;
#ifdef Q_OS_ANDROID
    // 窗口要在 Android 的主线程上改
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([keep] {
        constexpr jint KeepScreenOn = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        const QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        window.callMethod<void>(keep ? "addFlags" : "clearFlags", "(I)V", KeepScreenOn);
    });
#endif
    emit keepChanged();
}
