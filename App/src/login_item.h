#pragma once

#include <functional>

// 系统的登录项：当前用户登录（开机）时自动启动本程序。
// macOS 是 SMAppService.mainApp（login_item_mac.mm）；Windows 是当前用户的
// HKCU\Software\Microsoft\Windows\CurrentVersion\Run，命令是带引号的完整路径加 --autostart（login_item_win.cpp）
namespace login_item {

enum class Status {
    Off,
    On,
    // 登记了，但系统设置里没允许，登录时不会启动：macOS 要用户在"登录项"里允许（用户在那里关掉了也是这样）；
    // Windows 是用户在任务管理器或设置的"启动"里关掉了
    NeedsApproval,
};

Status status();
// 登记 / 取消登记。系统不让改时只写日志：改没改成看之后的 status()
void setEnabled(bool enabled);
// 打开系统设置里管登录时启动的那一页（macOS 的"登录项"，Windows 设置里的"启动"）
void openSystemSettings();
// 这一次是不是登录时由系统启动的：程序启动完（事件循环开始以后）调用一次 handler。
// 要在 QGuiApplication::exec() 之前调用：macOS 上要赶在系统发来"打开程序"的事件之前听着
void whenLaunched(std::function<void(bool atLogin)> handler);

} // namespace login_item
