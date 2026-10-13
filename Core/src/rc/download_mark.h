#pragma once

#include <QString>

namespace rc {

// 收到的文件标上"来自网络"（和浏览器下载的一样），打开收到的程序时系统会提醒：
// macOS 是隔离属性（LaunchServices 的 quarantine），Windows 是 Zone.Identifier（互联网区域）。
// 标不上（比如存在不支持的磁盘格式上）只写日志：文件已经收下了
void markDownloaded(const QString &path);

} // namespace rc
