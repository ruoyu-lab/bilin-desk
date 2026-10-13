#include "rc/download_mark.h"

namespace rc {

// Android 的文件不分来源，没有"来自网络"这种标记
void markDownloaded(const QString &)
{
}

} // namespace rc
