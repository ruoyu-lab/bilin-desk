#pragma once

#include <cstdint>
#include <functional>

// Windows 自己先拿走的键（Win、Win+V 打开剪贴板、Win+D 回桌面……），程序收不到。控制对方、画面有焦点时
// （RemoteView）用低级键盘钩子把 Win 键截下来交给 onKey（HID 键盘用法、按下还是松开），本机不再处理它：
// Win+V 发到对方是对方的 Win+V（Mac 上是 Command+V）。同时只有一个；Win+L、Ctrl+Alt+Del 系统不让截
class SystemKeys
{
public:
    explicit SystemKeys(std::function<void(uint32_t usage, bool down)> onKey);
    ~SystemKeys();

    SystemKeys(const SystemKeys &) = delete;
    SystemKeys &operator=(const SystemKeys &) = delete;

private:
    std::function<void(uint32_t, bool)> onKey_;
};
