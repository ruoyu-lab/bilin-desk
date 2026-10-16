#include "phone_system.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include <string>

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

void hideHomeIndicator()
{
    // 窗口的根视图控制器是 Qt 的，这两样用的系统默认（横条一直在、底边的手势先给系统）：给它的类换掉，再让系统重新问一遍。
    // 窗口刚建好时还没挂到场景上，所以窗口每次变成活动的都调一下（换过了就只让系统重新问）
    const std::string returnsBool = std::string(@encode(BOOL)) + "@:";
    const std::string returnsEdge = std::string(@encode(UIRectEdge)) + "@:";
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class])
            continue;
        for (UIWindow *window in static_cast<UIWindowScene *>(scene).windows) {
            UIViewController *const controller = window.rootViewController;
            if (controller == nil)
                continue;
            // 只有一个窗口：换一次就行
            static bool replaced = false;
            if (!replaced) {
                class_replaceMethod(controller.class, @selector(prefersHomeIndicatorAutoHidden),
                                    imp_implementationWithBlock(^BOOL(id) { return YES; }), returnsBool.c_str());
                class_replaceMethod(controller.class, @selector(preferredScreenEdgesDeferringSystemGestures),
                                    imp_implementationWithBlock(^UIRectEdge(id) { return UIRectEdgeBottom; }),
                                    returnsEdge.c_str());
                replaced = true;
            }
            [controller setNeedsUpdateOfHomeIndicatorAutoHidden];
            [controller setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
        }
    }
}

void captureSystemKeys(bool)
{
}

} // namespace phone_system
