#pragma once

#include "app_keeper.h"

#include <QByteArray>
#include <QString>

#include <functional>

// 装好时后台服务怎么运行（rc/system_service.h、docs/install-and-first-run.md）。各平台一份：
// system_mode_win.cpp、system_mode_mac.cpp
namespace system_mode {

// 数据目录（rc::system_service::stateDirectory）：没有就建好，只有 SYSTEM / root 和管理员能进；有了核对确实是这样
// （普通用户抢先建好的，里面可能放了他的身份）。macOS 还有放套接字的目录。不对时返回为什么
QString prepareStateDirectory();
// 程序和它所在的目录只有管理员能改：不然别人换掉它，它就以 SYSTEM / root 运行了。不对时返回为什么
QString programProblem();
// 给控制台前的用户启动界面程序（不弹窗口，rc/programs.h 的 BackgroundArgument）
AppKeeper::Launch launchApp();
// 要结束时（Windows：看护者要它结束，服务停了、控制台换了会话；macOS：launchd 发来 SIGTERM）在主线程上调 stop
void onStopRequest(std::function<void()> stop);
// 启动安装程序装新版本（Updater 下载、核对过的 rc::system_service::updateInstaller）。装的时候后台服务会被停掉、
// 换掉，安装程序不能跟着它结束：Windows 交给系统服务那个进程启动（看护者作业里的进程跟着看护者一起结束）；
// macOS 交给 launchd 当一个单独的任务。启动了时 true
bool installUpdate();

#ifdef Q_OS_WIN
// 系统服务本身（服务控制管理器启动的，rc/programs.h 的 SystemServiceArgument）：看护者在控制台会话里一直开着一份
// 后台服务（HelperArgument），到服务停下为止
int runSystemService();
#else
// 后台服务和各个会话里的助手之间的链接密钥（rc::system_service::agentKey）：没有就生成一个
QByteArray agentKey();
#endif

} // namespace system_mode
