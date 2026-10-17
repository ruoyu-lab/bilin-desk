#pragma once

// macOS 自己先拿走的键（F11 显示桌面、Command+空格、Control+方向键、Command+Tab、截屏……），程序收不到：
// 控制对方、画面有焦点时（RemoteView）暂停系统的这些快捷键，按键照常送到画面、发给对方。
// 用的是 SkyLight 里不公开的 CGSSetGlobalHotKeyOperatingMode：不要授权，只管本进程，进程没了系统自己恢复
namespace system_keys_mac {

void pauseShortcuts(bool paused);

} // namespace system_keys_mac
