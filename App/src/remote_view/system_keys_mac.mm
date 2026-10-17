#include "system_keys_mac.h"

#import <ApplicationServices/ApplicationServices.h>

extern "C" {
typedef int CGSConnectionID;
CGSConnectionID CGSMainConnectionID(void);
CGError CGSSetGlobalHotKeyOperatingMode(CGSConnectionID connection, int mode);
}

namespace system_keys_mac {

namespace {
constexpr int Enabled = 0;  // kCGSGlobalHotKeyEnable
constexpr int Disabled = 1; // kCGSGlobalHotKeyDisable
} // namespace

void pauseShortcuts(bool paused)
{
    CGSSetGlobalHotKeyOperatingMode(CGSMainConnectionID(), paused ? Disabled : Enabled);
}

} // namespace system_keys_mac
