#include "remote_input.h"

#include <rcsdk/rcsdk.h>

#include <QKeyEvent>
#include <QtGlobal>

#include <utility>

namespace remote_input {

namespace {

// HID 键盘用法里用到的几个
constexpr uint32_t KeyD = 7;
constexpr uint32_t KeyE = 8;
constexpr uint32_t KeyQ = 20;
constexpr uint32_t KeyR = 21;
constexpr uint32_t KeyS = 22;
constexpr uint32_t Key5 = 34;
constexpr uint32_t Escape = 41;
constexpr uint32_t Tab = 43;
constexpr uint32_t Space = 44;
constexpr uint32_t LeftControl = 224;
constexpr uint32_t LeftShift = 225;
constexpr uint32_t LeftAlt = 226;
constexpr uint32_t LeftGui = 227; // Mac 的 Command，Windows 的 Win
constexpr uint32_t RightControl = 228;
constexpr uint32_t RightGui = 231;

constexpr Shortcut MacShortcuts[] = {
    {"appSwitcher", QT_TRANSLATE_NOOP("RemoteView", "切换应用"), "⌘Tab", {LeftGui, Tab}},
    {"spotlight", QT_TRANSLATE_NOOP("RemoteView", "聚焦搜索"), "⌘空格", {LeftGui, Space}},
    {"forceQuit", QT_TRANSLATE_NOOP("RemoteView", "强制退出应用"), "⌥⌘Esc", {LeftAlt, LeftGui, Escape}},
    {"screenshot", QT_TRANSLATE_NOOP("RemoteView", "截屏"), "⇧⌘5", {LeftShift, LeftGui, Key5}},
    {"lock", QT_TRANSLATE_NOOP("RemoteView", "锁定屏幕"), "⌃⌘Q", {LeftControl, LeftGui, KeyQ}},
};

constexpr Shortcut WindowsShortcuts[] = {
    {"start", QT_TRANSLATE_NOOP("RemoteView", "开始菜单"), "Win", {LeftGui}},
    {"windowSwitcher", QT_TRANSLATE_NOOP("RemoteView", "切换窗口"), "Alt+Tab", {LeftAlt, Tab}},
    {"taskView", QT_TRANSLATE_NOOP("RemoteView", "任务视图"), "Win+Tab", {LeftGui, Tab}},
    {"desktop", QT_TRANSLATE_NOOP("RemoteView", "显示桌面"), "Win+D", {LeftGui, KeyD}},
    {"explorer", QT_TRANSLATE_NOOP("RemoteView", "文件资源管理器"), "Win+E", {LeftGui, KeyE}},
    {"run", QT_TRANSLATE_NOOP("RemoteView", "运行"), "Win+R", {LeftGui, KeyR}},
    {"taskManager", QT_TRANSLATE_NOOP("RemoteView", "任务管理器"), "Ctrl+Shift+Esc", {LeftControl, LeftShift, Escape}},
    {"screenshot", QT_TRANSLATE_NOOP("RemoteView", "截图"), "Win+Shift+S", {LeftGui, LeftShift, KeyS}},
};

template<size_t N>
uint32_t lookUp(const std::pair<uint16_t, uint8_t> (&table)[N], uint32_t code)
{
    for (const auto &[from, usage] : table) {
        if (from == code)
            return usage;
    }
    return 0;
}

#ifdef Q_OS_WIN
// 扫描码（第 1 套，WM_KEYDOWN 的 lParam 16..23 位），扩展键加 0xE000（lParam 24 位）
constexpr std::pair<uint16_t, uint8_t> ScanCodes[] = {
    {0x0001, 41},  {0x0002, 30},  {0x0003, 31},  {0x0004, 32},  {0x0005, 33},  {0x0006, 34},  {0x0007, 35},
    {0x0008, 36},  {0x0009, 37},  {0x000A, 38},  {0x000B, 39},  {0x000C, 45},  {0x000D, 46},  {0x000E, 42},
    {0x000F, 43},  {0x0010, 20},  {0x0011, 26},  {0x0012, 8},   {0x0013, 21},  {0x0014, 23},  {0x0015, 28},
    {0x0016, 24},  {0x0017, 12},  {0x0018, 18},  {0x0019, 19},  {0x001A, 47},  {0x001B, 48},  {0x001C, 40},
    {0x001D, 224}, {0x001E, 4},   {0x001F, 22},  {0x0020, 7},   {0x0021, 9},   {0x0022, 10},  {0x0023, 11},
    {0x0024, 13},  {0x0025, 14},  {0x0026, 15},  {0x0027, 51},  {0x0028, 52},  {0x0029, 53},  {0x002A, 225},
    {0x002B, 49},  {0x002C, 29},  {0x002D, 27},  {0x002E, 6},   {0x002F, 25},  {0x0030, 5},   {0x0031, 17},
    {0x0032, 16},  {0x0033, 54},  {0x0034, 55},  {0x0035, 56},  {0x0036, 229}, {0x0037, 85},  {0x0038, 226},
    {0x0039, 44},  {0x003A, 57},  {0x003B, 58},  {0x003C, 59},  {0x003D, 60},  {0x003E, 61},  {0x003F, 62},
    {0x0040, 63},  {0x0041, 64},  {0x0042, 65},  {0x0043, 66},  {0x0044, 67},  {0x0045, 72},  {0x0046, 71},
    {0x0047, 95},  {0x0048, 96},  {0x0049, 97},  {0x004A, 86},  {0x004B, 92},  {0x004C, 93},  {0x004D, 94},
    {0x004E, 87},  {0x004F, 89},  {0x0050, 90},  {0x0051, 91},  {0x0052, 98},  {0x0053, 99},  {0x0056, 100},
    {0x0057, 68},  {0x0058, 69},  {0x0059, 103}, {0x0064, 104}, {0x0065, 105}, {0x0066, 106}, {0x0067, 107},
    {0x0068, 108}, {0x0069, 109}, {0x006A, 110}, {0x006B, 111}, {0x006C, 112}, {0x006D, 113}, {0x006E, 114},
    {0x0070, 136}, {0x0071, 145}, {0x0072, 144}, {0x0073, 135}, {0x0076, 115}, {0x0079, 138}, {0x007B, 139},
    {0x007D, 137}, {0x007E, 133}, {0xE01C, 88},  {0xE01D, 228}, {0xE020, 127}, {0xE02E, 129}, {0xE030, 128},
    {0xE035, 84},  {0xE037, 70},  {0xE038, 230}, {0xE045, 83},  {0xE047, 74},  {0xE048, 82},  {0xE049, 75},
    {0xE04B, 80},  {0xE04D, 79},  {0xE04F, 77},  {0xE050, 81},  {0xE051, 78},  {0xE052, 73},  {0xE053, 76},
    {0xE05B, 227}, {0xE05C, 231}, {0xE05D, 101},
};
#else
// macOS 的虚拟键码（kVK_*，NSEvent.keyCode）；Fn 没有对应的用法
constexpr std::pair<uint16_t, uint8_t> MacKeyCodes[] = {
    {0x00, 4},   {0x01, 22},  {0x02, 7},   {0x03, 9},   {0x04, 11},  {0x05, 10},  {0x06, 29},  {0x07, 27},
    {0x08, 6},   {0x09, 25},  {0x0A, 100}, {0x0B, 5},   {0x0C, 20},  {0x0D, 26},  {0x0E, 8},   {0x0F, 21},
    {0x10, 28},  {0x11, 23},  {0x12, 30},  {0x13, 31},  {0x14, 32},  {0x15, 33},  {0x16, 35},  {0x17, 34},
    {0x18, 46},  {0x19, 38},  {0x1A, 36},  {0x1B, 45},  {0x1C, 37},  {0x1D, 39},  {0x1E, 48},  {0x1F, 18},
    {0x20, 24},  {0x21, 47},  {0x22, 12},  {0x23, 19},  {0x24, 40},  {0x25, 15},  {0x26, 13},  {0x27, 52},
    {0x28, 14},  {0x29, 51},  {0x2A, 49},  {0x2B, 54},  {0x2C, 56},  {0x2D, 17},  {0x2E, 16},  {0x2F, 55},
    {0x30, 43},  {0x31, 44},  {0x32, 53},  {0x33, 42},  {0x35, 41},  {0x36, 231}, {0x37, 227}, {0x38, 225},
    {0x39, 57},  {0x3A, 226}, {0x3B, 224}, {0x3C, 229}, {0x3D, 230}, {0x3E, 228}, {0x40, 108}, {0x41, 99},
    {0x43, 85},  {0x45, 87},  {0x47, 83},  {0x4B, 84},  {0x4C, 88},  {0x4E, 86},  {0x4F, 109}, {0x50, 110},
    {0x51, 103}, {0x52, 98},  {0x53, 89},  {0x54, 90},  {0x55, 91},  {0x56, 92},  {0x57, 93},  {0x58, 94},
    {0x59, 95},  {0x5A, 111}, {0x5B, 96},  {0x5C, 97},  {0x5D, 137}, {0x5E, 135}, {0x5F, 133}, {0x60, 62},
    {0x61, 63},  {0x62, 64},  {0x63, 60},  {0x64, 65},  {0x65, 66},  {0x66, 145}, {0x67, 68},  {0x68, 144},
    {0x69, 104}, {0x6A, 107}, {0x6B, 105}, {0x6D, 67},  {0x6E, 101}, {0x6F, 69},  {0x71, 106}, {0x72, 73},
    {0x73, 74},  {0x74, 75},  {0x75, 76},  {0x76, 61},  {0x77, 77},  {0x78, 59},  {0x79, 78},  {0x7A, 58},
    {0x7B, 80},  {0x7C, 79},  {0x7D, 81},  {0x7E, 82},
};
#endif

} // namespace

uint32_t keyUsage(const QKeyEvent &key, bool remoteIsMac)
{
#ifdef Q_OS_WIN
    Q_UNUSED(remoteIsMac);
    // Qt 给的扫描码：低 8 位是码，第 8 位是扩展键
    const quint32 scan = key.nativeScanCode();
    return lookUp(ScanCodes, (scan & 0xff) | ((scan & 0x100) ? 0xE000u : 0u));
#else
    const uint32_t usage = lookUp(MacKeyCodes, key.nativeVirtualKey());
    if (remoteIsMac)
        return usage;
    switch (usage) {
    case LeftGui: return LeftControl;
    case LeftControl: return LeftGui;
    case RightGui: return RightControl;
    case RightControl: return RightGui;
    default: return usage;
    }
#endif
}

uint32_t namedKeyUsage(QStringView name)
{
    static constexpr std::pair<QLatin1StringView, uint8_t> Named[] = {
        {QLatin1StringView("Enter"), 40},    {QLatin1StringView("Escape"), 41}, {QLatin1StringView("Backspace"), 42},
        {QLatin1StringView("Tab"), 43},      {QLatin1StringView("Space"), 44},  {QLatin1StringView("Home"), 74},
        {QLatin1StringView("PageUp"), 75},   {QLatin1StringView("Delete"), 76}, {QLatin1StringView("End"), 77},
        {QLatin1StringView("PageDown"), 78}, {QLatin1StringView("Right"), 79},  {QLatin1StringView("Left"), 80},
        {QLatin1StringView("Down"), 81},     {QLatin1StringView("Up"), 82},     {QLatin1StringView("Control"), LeftControl},
        {QLatin1StringView("Shift"), LeftShift}, {QLatin1StringView("Alt"), LeftAlt}, {QLatin1StringView("Meta"), LeftGui},
    };
    for (const auto &[key, usage] : Named) {
        if (name == key)
            return usage;
    }
    // F1~F12：58~69
    if (name.size() >= 2 && name.front() == u'F') {
        bool ok = false;
        const int number = name.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 12)
            return 57 + number;
    }
    if (name.size() == 1) {
        const char16_t c = QChar(name.front()).toLower().unicode();
        if (c >= u'a' && c <= u'z')
            return 4 + (c - u'a');
        if (c >= u'1' && c <= u'9')
            return 30 + (c - u'1');
        if (c == u'0')
            return 39;
    }
    return 0;
}

uint32_t button(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return RCS_BUTTON_LEFT;
    case Qt::RightButton: return RCS_BUTTON_RIGHT;
    case Qt::MiddleButton: return RCS_BUTTON_MIDDLE;
    case Qt::BackButton: return RCS_BUTTON_BACK;
    case Qt::ForwardButton: return RCS_BUTTON_FORWARD;
    default: return 0;
    }
}

std::span<const Shortcut> shortcuts(bool remoteIsMac)
{
    if (remoteIsMac)
        return MacShortcuts;
    return WindowsShortcuts;
}

int WheelLines::take(int angleDelta, int linesPerNotch)
{
    rest_ += angleDelta * linesPerNotch;
    const int lines = rest_ / 120;
    rest_ -= lines * 120;
    return lines;
}

} // namespace remote_input
