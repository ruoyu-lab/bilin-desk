#include "system_keys_win.h"

#include <QtGlobal>

#include <windows.h>

namespace {

SystemKeys *active = nullptr;
std::function<void(uint32_t, bool)> *deliver = nullptr;
HHOOK hook = nullptr;

constexpr uint32_t LeftGui = 227;
constexpr uint32_t RightGui = 231;

// 在装钩子的线程（界面线程）的消息循环里调
LRESULT CALLBACK onLowLevelKey(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && deliver) {
        const auto *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
        if (key->vkCode == VK_LWIN || key->vkCode == VK_RWIN) {
            const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            (*deliver)(key->vkCode == VK_LWIN ? LeftGui : RightGui, down);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

} // namespace

SystemKeys::SystemKeys(std::function<void(uint32_t, bool)> onKey)
    : onKey_(std::move(onKey))
{
    Q_ASSERT(!active);
    active = this;
    deliver = &onKey_;
    hook = SetWindowsHookExW(WH_KEYBOARD_LL, onLowLevelKey, GetModuleHandleW(nullptr), 0);
    if (!hook)
        qWarning("装不了键盘钩子（%lu）：Win 组合键还是本机自己处理", GetLastError());
}

SystemKeys::~SystemKeys()
{
    if (hook)
        UnhookWindowsHookEx(hook);
    hook = nullptr;
    deliver = nullptr;
    active = nullptr;
}
