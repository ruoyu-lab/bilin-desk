#pragma once

#include "setup_steps.h"

#include <QString>

#include <optional>
#include <vector>

// 安装、卸载在各个系统上怎么做（setup_win.cpp、setup_mac.mm）
namespace setup_platform {

// 命令行里的安装模式（Windows）：--setup install（Setup.exe 解压以后启动，source 是解压出来的程序目录）、
// --setup uninstall（应用和功能里点卸载）。卸载要管理员身份、要能删掉程序目录：先把程序复制到临时目录，
// 用管理员身份从那里再启动一次，带上 --target 程序装在哪、--data 用户的数据在哪。
// --quiet：不要界面，做完就退出（给管理员批量部署：Bilin-Setup.exe --quiet；没成时退出码 1）
struct Request
{
    enum class Mode { Install, Uninstall };
    Mode mode = Mode::Install;
    QString target;
    QString data;
    bool quiet = false;
};
std::optional<Request> requestFromArguments(const QStringList &arguments);

// 装好的那一份在哪里（Windows：应用和功能里登记的；macOS："应用程序"里的）；没装过时为空
QString installedPath();
// 现在运行的这一份就是装好的那一份（不是开发版本、不是 Setup.exe 解压出来的）
bool runningInstalled();
// 第一次装时默认装到哪
QString defaultInstallPath();
// 桌面上有没有它的快捷方式（Windows）：升级时照旧，第一次装默认有
bool hasDesktopShortcut();

// Windows：卸载这一次启动是不是还得先换到临时目录、用管理员身份再启动（是的话就去做，返回 true，这一次退出）
bool relaunchForUninstall(const Request &request);
// Windows：安装要管理员身份；没有时用管理员身份再启动自己（返回 true，这一次退出）
bool relaunchElevated();

// desktopShortcut：桌面上要不要快捷方式（Windows，所有用户的桌面）
std::vector<SetupStep> installSteps(const QString &source, const QString &target, bool desktopShortcut);
std::vector<SetupStep> uninstallSteps(const Request &request);

// 装好以后打开它（Windows 上不带安装程序的管理员身份）
void openInstalled(const QString &target);
// 卸载完了：程序自己的临时那一份等退出以后删掉（Windows）
void cleanUpAfterExit();

} // namespace setup_platform
