#include "dock.h"

#import <AppKit/AppKit.h>

// 只处理"重新打开"这一个 Apple Event（点 Dock 图标、再次从访达打开），
// 不用"程序变成活动的"：点一下对话窗口、刘海也会让程序变成活动的
@interface RCReopenHandler : NSObject
@property(nonatomic) std::function<void()> handler;
@end

@implementation RCReopenHandler
- (void)handleReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply
{
    if (self.handler)
        self.handler();
}
@end

void installDockReopenHandler(std::function<void()> handler)
{
    static RCReopenHandler *target = [[RCReopenHandler alloc] init];
    target.handler = std::move(handler);
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:target
                                                       andSelector:@selector(handleReopen:withReplyEvent:)
                                                     forEventClass:kCoreEventClass
                                                        andEventID:kAEReopenApplication];
}

void setDockIconVisible(bool visible)
{
    const NSApplicationActivationPolicy policy = visible ? NSApplicationActivationPolicyRegular
                                                         : NSApplicationActivationPolicyAccessory;
    if (NSApp.activationPolicy != policy)
        NSApp.activationPolicy = policy;
}
