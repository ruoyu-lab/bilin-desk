#pragma once

#include <QLatin1StringView>
#include <QString>

// 装好的程序里的系统服务（docs/install-and-first-run.md），开机就运行、没人登录时也在：
// - Windows：服务 WindowsName（LocalSystem），里面是 SDK 的看护者，在控制台会话里以 SYSTEM 身份启动后台服务
//   （rc/programs.h 的 HelperArgument）
// - macOS：LaunchDaemon DaemonLabel（root）运行后台服务（DaemonArgument），持有连接；LaunchAgent AgentLabel
//   在登录窗口和每个用户的会话里运行助手（rc/programs.h 的 Agent），录屏、声音、键鼠在它那里
// 开发时编的程序不装这些：后台服务以当前用户运行，界面程序启动它（rc/paths.h、rc/ipc/endpoint.h）
namespace rc::system_service {

#ifdef Q_OS_WIN
inline constexpr wchar_t WindowsName[] = L"Bilin";
#else
inline constexpr char DaemonLabel[] = "com.bilindesk.bilin.service";
inline constexpr char AgentLabel[] = "com.bilindesk.bilin.agent";
#endif

// 装了没有（Windows：服务在不在；macOS：LaunchDaemon 的 plist 在不在）
bool installed();
// 装好时后台服务的本机连接，整台电脑一个（每个用户的界面程序都连它）
QString endpoint();
// 装好时后台服务的数据（本机身份、设置、日志），整台电脑一份，只有 SYSTEM / root 和管理员能读写
QString stateDirectory();
// 自己更新时下载、核对过的安装包（Service/src/updater.h），在数据目录里
QString updateInstaller();

#ifdef Q_OS_MACOS
QString daemonPlist();
QString agentPlist();
// 后台服务和助手之间的链接（SDK 的 agent link）：分享会话 access（rc/transport.h）的那个，Unix 套接字的路径
QString agentEndpoint(QLatin1StringView access);
// 链接的密钥：每个用户的助手都要读得到
QString agentKey();
#endif

} // namespace rc::system_service
