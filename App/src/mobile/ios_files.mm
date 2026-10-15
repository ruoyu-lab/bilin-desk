#include "ios_files.h"

#include <QStandardPaths>

#import <UIKit/UIKit.h>

// 快速预览从哪个界面弹出来：程序窗口最上面的那个
@interface BilinPreviewDelegate : NSObject <UIDocumentInteractionControllerDelegate>
@end

@implementation BilinPreviewDelegate

- (UIViewController *)documentInteractionControllerViewControllerForPreview:(UIDocumentInteractionController *)controller
{
    Q_UNUSED(controller);
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class])
            continue;
        UIViewController *top = static_cast<UIWindowScene *>(scene).keyWindow.rootViewController;
        while (top.presentedViewController)
            top = top.presentedViewController;
        if (top)
            return top;
    }
    return nil;
}

@end

namespace ios_files {

QString downloads()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void open(const QString &path)
{
    // 预览开着的时候系统要一直拿着它和它的代理：同一时间只有一个，换下一个时放掉上一个
    static UIDocumentInteractionController *controller = nil;
    static BilinPreviewDelegate *const delegate = [[BilinPreviewDelegate alloc] init];
    [controller release];
    controller = [[UIDocumentInteractionController interactionControllerWithURL:[NSURL fileURLWithPath:path.toNSString()]] retain];
    controller.delegate = delegate;
    [controller presentPreviewAnimated:YES];
}

} // namespace ios_files
