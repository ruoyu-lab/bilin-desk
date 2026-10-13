#include "quit_reason.h"

#import <AppKit/AppKit.h>

bool quitAskedByUser()
{
    // 系统要退出程序时发来"退出"的 Apple Event，带着原因（注销、重新启动、关机）；
    // Cmd+Q 没有 Apple Event，Dock 里的"退出"有但没有原因
    NSAppleEventDescriptor *event = NSAppleEventManager.sharedAppleEventManager.currentAppleEvent;
    return event == nil || event.eventID != kAEQuitApplication || [event attributeDescriptorForKeyword:kAEQuitReason] == nil;
}
