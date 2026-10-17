#include "hardware_keys.h"

#include "remote_input.h"
#include "remote_view.h"

#include <QHash>
#include <QPointer>
#include <QTimer>

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace hardware_keys {
namespace {

// 按住多久开始连发、连发多快（毫秒）
constexpr int RepeatDelayMs = 500;
constexpr int RepeatIntervalMs = 33;
constexpr uint32_t CapsLock = 57;
constexpr long KeyF = 9;
constexpr uint32_t FirstModifier = 224; // 左 Control ~ 右 Command：224~231

// 按下的键发给了哪个画面：松开也给它（中途焦点换了也是）
struct Pressed
{
    QPointer<RemoteView> view;
    uint32_t usage = 0; // 发出去的（Command、Control 对换以后）
};

QHash<long, Pressed> pressed; // 按 UIKey.keyCode
long repeating = 0;           // 正在连发的键（最后按下的那个，同系统键盘）
QTimer *repeatTimer = nullptr;
IMP sendEvent = nullptr;

void startRepeat(long code)
{
    repeating = code;
    repeatTimer->start(RepeatDelayMs);
}

void repeatKey()
{
    const auto it = pressed.constFind(repeating);
    if (it == pressed.cend() || !it->view) {
        repeatTimer->stop();
        return;
    }
    it->view->sendKey(it->usage, true, true);
    repeatTimer->start(RepeatIntervalMs);
}

void release(long code)
{
    const Pressed key = pressed.take(code);
    if (code == repeating)
        repeatTimer->stop();
    if (key.view)
        key.view->sendKey(key.usage, false, false);
}

// 这个事件里的键都发给对方了：不再交给 UIKit（Qt、输入法）
bool handle(UIPressesEvent *event)
{
    bool handled = false;
    for (UIPress *press in event.allPresses) {
        UIKey *const key = press.key;
        if (key == nil)
            continue;
        const long code = key.keyCode;
        switch (press.phase) {
        case UIPressPhaseBegan: {
            // 全屏时退出的组合键（RemoteView::isExitFullScreenChord）：不发给对方，松开也不发
            const UIKeyModifierFlags flags = key.modifierFlags;
            if (RemoteView *const full = RemoteView::focused(); full && full->fullScreen() && code == KeyF
                && (flags & UIKeyModifierShift) && (flags & UIKeyModifierAlternate)
                && (flags & (UIKeyModifierControl | UIKeyModifierCommand))) {
                pressed.insert(code, Pressed{});
                emit full->exitFullScreenRequested();
                handled = true;
                break;
            }
            RemoteView *const view = RemoteView::keyTarget();
            if (view == nullptr)
                break;
            const uint32_t usage = remote_input::appleKeyboardUsage(uint32_t(code), view->remoteIsMac());
            if (!usage)
                break;
            pressed.insert(code, Pressed{view, usage});
            view->sendKey(usage, true, false);
            if (usage != CapsLock && usage < FirstModifier)
                startRepeat(code);
            handled = true;
            break;
        }
        case UIPressPhaseEnded:
        case UIPressPhaseCancelled:
            if (pressed.contains(code)) {
                release(code);
                handled = true;
            }
            break;
        default:
            break;
        }
    }
    return handled;
}

} // namespace

void install()
{
    if (sendEvent != nullptr)
        return;
    repeatTimer = new QTimer(qApp);
    repeatTimer->setSingleShot(true);
    QObject::connect(repeatTimer, &QTimer::timeout, repeatKey);
    // 所有事件都经过 -[UIApplication sendEvent:]：换成先看一眼按键的，其余照旧
    const Method method = class_getInstanceMethod(UIApplication.class, @selector(sendEvent:));
    sendEvent = method_getImplementation(method);
    method_setImplementation(method, imp_implementationWithBlock(^(UIApplication *application, UIEvent *event) {
        if (event.type == UIEventTypePresses && handle(static_cast<UIPressesEvent *>(event)))
            return;
        reinterpret_cast<void (*)(id, SEL, UIEvent *)>(sendEvent)(application, @selector(sendEvent:), event);
    }));
}

} // namespace hardware_keys
