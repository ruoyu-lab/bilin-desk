#include "rc/device_name.h"

#include <QSysInfo>

#include <SystemConfiguration/SystemConfiguration.h>

namespace rc {

QString defaultDeviceName()
{
    // 主机名是"lichas-MacBook-Pro.local"这样的；用户看到、改过的是电脑名称
    CFStringRef name = SCDynamicStoreCopyComputerName(nullptr, nullptr);
    if (!name)
        return QSysInfo::machineHostName();
    const QString result = QString::fromCFString(name);
    CFRelease(name);
    return result;
}

} // namespace rc
