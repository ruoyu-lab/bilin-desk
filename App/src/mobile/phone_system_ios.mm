#include "phone_system.h"

#import <UIKit/UIKit.h>

namespace phone_system {

void keepScreenOn(bool keep)
{
    UIApplication.sharedApplication.idleTimerDisabled = keep;
}

void setDark(bool dark)
{
    // 程序的窗口按程序选的外观画系统的部分：状态栏的字（Qt 铺到状态栏底下时用默认样式，跟着窗口的外观）、键盘
    const UIUserInterfaceStyle style = dark ? UIUserInterfaceStyleDark : UIUserInterfaceStyleLight;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class])
            continue;
        for (UIWindow *window in static_cast<UIWindowScene *>(scene).windows)
            window.overrideUserInterfaceStyle = style;
    }
}

void pressFeedback()
{
    UIImpactFeedbackGenerator *const generator =
        [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
    [generator impactOccurred];
    [generator release];
}

} // namespace phone_system
