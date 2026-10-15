#include "rc/device_name.h"

#import <UIKit/UIKit.h>

namespace rc {

QString defaultDeviceName()
{
    // iOS 16 起不给"设置 › 通用 › 关于本机"里的名字（要苹果另批的权限），只给"iPad""iPhone"：用户可以在设置里改
    return QString::fromNSString(UIDevice.currentDevice.name);
}

} // namespace rc
