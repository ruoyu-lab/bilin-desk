#pragma once

#include <QString>

// Android 上收到的文件（FileTransferList）：要放在文件管理器里找得到的地方
namespace android_files {

// 公共的"下载"里的"比邻"（Android 11 起程序不用授权就能在里面建文件）；Android 10 不让，存在程序自己的下载目录里
QString downloads();

} // namespace android_files
