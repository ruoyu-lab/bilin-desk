#include "phone_system.h"

#include <QCoreApplication>
#include <QJniObject>

namespace phone_system {

namespace {

// Android 的窗口要在它的主线程上改
template<typename Change>
void changeWindow(Change change)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([change] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        change(activity.callObjectMethod("getWindow", "()Landroid/view/Window;"));
    });
}

} // namespace

void keepScreenOn(bool keep)
{
    changeWindow([keep](const QJniObject &window) {
        constexpr jint KeepScreenOn = 128; // WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        window.callMethod<void>(keep ? "addFlags" : "clearFlags", "(I)V", KeepScreenOn);
    });
}

void setDark(bool dark)
{
    changeWindow([dark](const QJniObject &window) {
        // 浅色时状态栏、导航条的图标是黑的（APPEARANCE_LIGHT_STATUS_BARS | APPEARANCE_LIGHT_NAVIGATION_BARS）；
        // Android 10 还没有 WindowInsetsController，用 SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | LIGHT_NAVIGATION_BAR
        constexpr jint AndroidR = 30;
        if (QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT") >= AndroidR) {
            constexpr jint Light = 8 | 16;
            window.callObjectMethod("getInsetsController", "()Landroid/view/WindowInsetsController;")
                .callMethod<void>("setSystemBarsAppearance", "(II)V", dark ? 0 : Light, Light);
            return;
        }
        constexpr jint Light = 0x2000 | 0x10;
        const QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        const jint flags = decor.callMethod<jint>("getSystemUiVisibility");
        decor.callMethod<void>("setSystemUiVisibility", "(I)V", dark ? flags & ~Light : flags | Light);
    });
}

void hideHomeIndicator()
{
}

void pressFeedback()
{
    changeWindow([](const QJniObject &window) {
        constexpr jint LongPress = 0; // HapticFeedbackConstants.LONG_PRESS
        window.callObjectMethod("getDecorView", "()Landroid/view/View;")
            .callMethod<jboolean>("performHapticFeedback", "(I)Z", LongPress);
    });
}

} // namespace phone_system
