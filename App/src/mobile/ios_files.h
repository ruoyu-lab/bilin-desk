#pragma once

#include <QString>

// iOS 上收到的文件：存在程序的"文稿"目录（"文件"App 的"我的 iPad › 比邻"，Info.plist 的 UIFileSharingEnabled）；
// 没有"在文件夹中显示"，点"打开"用系统的快速预览看（右上角能分享、存到别处、用别的程序打开）
namespace ios_files {

QString downloads();
void open(const QString &path);

} // namespace ios_files
