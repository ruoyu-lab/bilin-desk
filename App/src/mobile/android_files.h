#pragma once

#include <QString>
#include <QUrl>

namespace rc {
class InterruptedTransfers;
}

// Android 上的互传文件（FileTransferList）：SDK 按路径读写文件，Android 的文件选择器给的却是 content:// 地址；
// 收到的文件要放在文件管理器里找得到的地方
namespace android_files {

// 收到的文件存到哪：公共的"下载"里的"比邻"（Android 11 起程序不用授权就能在里面建文件）；
// Android 10 不让，存在程序自己的下载目录里
QString downloads();

// 选来发送的文件（content:// 地址）复制到程序的缓存里，发这份；复制不了时为空
QString copyForSending(const QUrl &url);

// 缓存里复制出来的、已经不用接着发的删掉（程序启动时）
void removeStaleCopies(const rc::InterruptedTransfers &interrupted);

} // namespace android_files
