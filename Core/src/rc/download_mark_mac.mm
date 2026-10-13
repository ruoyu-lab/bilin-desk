#include "rc/download_mark.h"

#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

namespace rc {

void markDownloaded(const QString &path)
{
    // 后台服务没有 AppKit 的事件循环替它清自动释放池
    @autoreleasepool {
        NSURL *url = [NSURL fileURLWithPath:path.toNSString()];
        NSDictionary *properties = @{
            (__bridge NSString *)kLSQuarantineTypeKey: (__bridge NSString *)kLSQuarantineTypeOtherDownload,
            (__bridge NSString *)kLSQuarantineAgentNameKey: @"Bilin",
        };
        NSError *error = nil;
        if (![url setResourceValue:properties forKey:NSURLQuarantinePropertiesKey error:&error])
            qWarning("没能把 %s 标成来自网络：%s", qUtf8Printable(path), error.localizedDescription.UTF8String);
    }
}

} // namespace rc
