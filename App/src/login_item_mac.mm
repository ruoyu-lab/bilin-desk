#include "login_item.h"

#include <QtGlobal>

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>

namespace login_item {

Status status()
{
    switch (SMAppService.mainAppService.status) {
    case SMAppServiceStatusEnabled:
        return Status::On;
    case SMAppServiceStatusRequiresApproval:
        return Status::NeedsApproval;
    // 用户在系统设置里把它从"登录项"里删掉以后也可能是 notFound
    case SMAppServiceStatusNotRegistered:
    case SMAppServiceStatusNotFound:
        return Status::Off;
    }
    Q_UNREACHABLE_RETURN(Status::Off);
}

void setEnabled(bool enabled)
{
    SMAppService *service = SMAppService.mainAppService;
    NSError *error = nil;
    const BOOL done = enabled ? [service registerAndReturnError:&error] : [service unregisterAndReturnError:&error];
    if (!done)
        qWarning("改不了登录项（%ld）：%s", long(error.code), error.localizedDescription.UTF8String);
}

void openSystemSettings()
{
    [SMAppService openSystemSettingsLoginItems];
}

void whenLaunched(std::function<void(bool atLogin)> handler)
{
    // 系统启动程序时发来"打开程序"的 Apple Event，作为登录项启动的带着 keyAELaunchedAsLogInItem（SMAppService 启动不带参数）。
    // 它只在处理的时候读得到（currentAppleEvent），AppKit 正是在那时发"启动完了"的通知；Qt 在 exec() 里才让 AppKit 处理它
    __block id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                    [NSNotificationCenter.defaultCenter removeObserver:observer];
                    NSAppleEventDescriptor *event = NSAppleEventManager.sharedAppleEventManager.currentAppleEvent;
                    handler(event.eventID == kAEOpenApplication
                            && [event paramDescriptorForKeyword:keyAEPropData].enumCodeValue == keyAELaunchedAsLogInItem);
                }];
}

} // namespace login_item
