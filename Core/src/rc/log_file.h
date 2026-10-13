#pragma once

#include <QString>

namespace rc {

// 把程序的日志（qDebug、qWarning……）写进 directory 里的 name，同时照常输出到终端。
// 上一次运行的日志改名成 <name>.previous 留着，排查"上次为什么没起来"时用
void openLogFile(const QString &directory, const QString &name);

} // namespace rc
