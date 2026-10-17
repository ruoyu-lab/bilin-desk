#pragma once

#include <QStringView>
#include <Qt>

#include <cstdint>
#include <optional>
#include <span>

class QKeyEvent;

// 本机的键盘和鼠标 → SDK 的远程输入（rcs_input_event）
namespace remote_input {

// 按下的键 → USB HID 键盘用法（rcs_input_event.code）：按键在键盘上的位置，打出什么由对方的键盘布局决定。
// 取自系统给的原始键码（Windows 的扫描码、macOS 的虚拟键码）；不是对方能收的键时为 0。F1~F12 原样发（像这把键盘
// 插在对方电脑上）；静音、音量这些媒体键为 0，留在本机（对方的声音在本机放）。
// Mac 上的 Command 在不是 Mac 的对方那里当 Ctrl 用（Control 当 Win 键），Command+C 在那边也是复制
uint32_t keyUsage(const QKeyEvent &key, bool remoteIsMac);

#ifdef Q_OS_IOS
// iPad、iPhone 接着的键盘：UIKit 给的就是 USB HID 键码（UIKey.keyCode，hardware_keys_ios.mm），和 keyUsage 一样
// Command 在不是 Mac 的对方那里当 Ctrl 用；不是对方能收的键时为 0
uint32_t appleKeyboardUsage(uint32_t usage, bool remoteIsMac);
#endif

// 鼠标键 → RCS_BUTTON_*；SDK 不转发的键为 0
uint32_t button(Qt::MouseButton button);

// 手机的按键栏：按键的名字 → HID 键盘用法。名字是 Enter、Escape、Backspace、Tab、Space、Delete、Home、End、PageUp、
// PageDown、Left、Right、Up、Down、F1~F12、Control、Shift、Alt、Meta（Mac 的 Command、Windows 的 Win），
// 或者一个字母、数字（按着修饰键时打的）；不认识的为 0
uint32_t namedKeyUsage(QStringView name);

// 会话窗口"发送快捷键"菜单里的一项：对方系统上常用、本机按不出来（本机的系统先拿走了）的组合键
struct Shortcut
{
    const char *id;
    const char *label; // 菜单上的名字（QT_TRANSLATE_NOOP，上下文 RemoteView）
    const char *keys;  // 菜单上的按键，照对方系统的写法
    uint32_t usages[4]; // 依次按下的键（HID 键盘用法），松开时倒过来；用不满的是 0
};

// 对方是 Mac 时是 Mac 的组合键，否则是 Windows 的。Ctrl+Alt+Del 不在里面：发三个键产生不了，
// Windows 的锁屏 Win+L 也一样（系统不认程序发的）。Ctrl+Alt+Del 另外发（SecureAttention）
std::span<const Shortcut> shortcuts(bool remoteIsMac);

// "发送快捷键"菜单里的 Ctrl+Alt+Del：SDK 的 RCS_INPUT_SECURE_ATTENTION，对方装好了系统服务（Windows）才执行得了
inline constexpr char SecureAttention[] = "ctrlAltDel";

// 滚轮：Qt 的角度（一格 120）乘本机一格滚几行，换成 SDK 要的整行（rcs_input_event.wheel_x/y）；
// 触控板的小步一点点攒起来
class WheelLines
{
public:
    int take(int angleDelta, int linesPerNotch);

private:
    int rest_ = 0;
};

} // namespace remote_input
