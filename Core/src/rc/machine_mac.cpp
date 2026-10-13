#include "rc/machine.h"

#include <IOKit/IOKitLib.h>

namespace rc {

QString machineId()
{
    const io_service_t platform = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (!platform)
        return {};
    QString id;
    if (CFTypeRef uuid = IORegistryEntryCreateCFProperty(platform, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0)) {
        if (CFGetTypeID(uuid) == CFStringGetTypeID())
            id = QString::fromCFString(static_cast<CFStringRef>(uuid));
        CFRelease(uuid);
    }
    IOObjectRelease(platform);
    return id;
}

} // namespace rc
