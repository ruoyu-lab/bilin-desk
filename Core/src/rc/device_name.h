#pragma once

#include <QString>

namespace rc {

// 本机默认的名字（用户没在设置里改过时用它）：macOS 是"系统设置 › 通用 › 关于本机"里的名称，
// 其他系统是计算机名
QString defaultDeviceName();

} // namespace rc
