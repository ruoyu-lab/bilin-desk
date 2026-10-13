#pragma once

#include <functional>

// macOS 的 Dock。其他平台上什么都不做
//
// 程序没有窗口时点 Dock 图标（系统的"重新打开"事件），调用 handler
void installDockReopenHandler(std::function<void()> handler);
// 有窗口时 Dock 里有图标；窗口都关了只留菜单栏的图标，Dock 里不占位置
void setDockIconVisible(bool visible);
