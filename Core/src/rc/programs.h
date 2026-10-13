#pragma once

#include <QDir>
#include <QLatin1StringView>
#include <QString>

// 界面程序和后台服务放在一起，互相按名字找到、启动：Windows 上都在安装目录；macOS 上界面程序在 .app 的
// Contents/MacOS，后台服务和助手在 Contents/Helpers（不算 .app 的前台程序：用到系统的界面框架时 Dock 里也不多出图标，
// 它们各自带着说"不在 Dock 里"的 Info.plist，Service/platform/macos）
namespace rc::program {

// 后台服务、助手所在的目录：从界面程序所在的目录（applicationDirPath）找
inline QString helpersDirectory(const QString &appDirectory)
{
#ifdef Q_OS_MACOS
    return QDir(appDirectory + QStringLiteral("/../Helpers")).absolutePath();
#else
    return appDirectory;
#endif
}

// 界面程序所在的目录：从后台服务所在的目录找
inline QString appDirectory(const QString &helpersDirectory)
{
#ifdef Q_OS_MACOS
    return QDir(helpersDirectory + QStringLiteral("/../MacOS")).absolutePath();
#else
    return helpersDirectory;
#endif
}

// 界面程序（不带 .exe）
inline constexpr QLatin1StringView App{"Bilin"};
// 后台服务（不带 .exe）
inline constexpr QLatin1StringView Service{"BilinService"};
// macOS 装好时，各个会话里录屏、声音、键鼠的助手（rc/system_service.h）
inline constexpr QLatin1StringView Agent{"BilinAgent"};
// 启动界面程序时带上它：不弹窗口，只在托盘 / 菜单栏和刘海里。后台服务在有人连着本机、界面程序却没了时用
inline constexpr QLatin1StringView BackgroundArgument{"--background"};

// 后台服务怎么运行（rc/system_service.h）。都不带：开发版，以当前用户运行
// Windows：系统服务本身（服务控制管理器启动它），里面只有看护者
inline constexpr QLatin1StringView SystemServiceArgument{"--system-service"};
// Windows：看护者在控制台会话里以 SYSTEM 启动的那一份，就是后台服务
inline constexpr QLatin1StringView HelperArgument{"--helper"};
// macOS：LaunchDaemon 启动的那一份（root）
inline constexpr QLatin1StringView DaemonArgument{"--daemon"};

} // namespace rc::program
