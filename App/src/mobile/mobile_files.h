#pragma once

#include <QString>
#include <QUrl>

namespace rc {
class InterruptedTransfers;
}

// 手机上选来发送的文件（ViewerFiles）：SDK 按路径自己读文件，系统的文件选择器给的却是只有本程序经系统（Qt 的文件
// 接口）才读得到的地址：Android 是 content://，iOS 是临时授权的文件。先复制到程序的缓存里，发这一份
namespace mobile_files {

// 复制不了时为空。名字是文件选择器里看到的
QString copyForSending(const QUrl &url);

// 缓存里复制出来的、已经不用接着发的删掉（程序启动时）
void removeStaleCopies(const rc::InterruptedTransfers &interrupted);

} // namespace mobile_files
