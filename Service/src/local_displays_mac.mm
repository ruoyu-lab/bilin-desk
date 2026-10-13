#include "local_displays.h"

#import <AppKit/AppKit.h>

QHash<quint32, QString> displayNames()
{
    // SDK 在 macOS 上的 display_id 就是 CGDirectDisplayID，NSScreen 的 NSScreenNumber
    QHash<quint32, QString> names;
    @autoreleasepool {
        for (NSScreen *screen in NSScreen.screens) {
            NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
            names.insert(number.unsignedIntValue, QString::fromNSString(screen.localizedName));
        }
    }
    return names;
}
