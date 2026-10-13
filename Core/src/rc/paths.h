#pragma once

#include <QString>

namespace rc {

// 环境变量 RC_STATE_DIR：设了就用它当数据目录，本机连接的名字也跟着换（ipc/endpoint.h）。
// 测试时在同一台电脑上再跑一份完全隔开的（不碰正在用的那份）
inline constexpr char StateDirectoryVariable[] = "RC_STATE_DIR";

// 后台服务存本机身份、设置和日志的目录（当前用户的），没有就建好：
// macOS ~/Library/Application Support/Bilin，Windows %LOCALAPPDATA%\Bilin
QString stateDirectory();

} // namespace rc
