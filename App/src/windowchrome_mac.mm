#include "windowchrome.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtGui/qscreen_platform.h>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

// 只放一个弹性空白项：工具栏有内容时，系统才按标准高度布局标题栏和红绿灯
@interface RCToolbarDelegate : NSObject <NSToolbarDelegate>
@end

@implementation RCToolbarDelegate
- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return @[NSToolbarFlexibleSpaceItemIdentifier];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return @[NSToolbarFlexibleSpaceItemIdentifier];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSToolbarItemIdentifier)identifier
 willBeInsertedIntoToolbar:(BOOL)flag
{
    return nil;
}
@end

namespace {

NSWindow *nativeWindow(QWindow *window)
{
    if (!window)
        return nil;
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    return view.window;
}

NSScreen *nativeScreen(QScreen *screen)
{
    auto *cocoaScreen = screen ? screen->nativeInterface<QNativeInterface::QCocoaScreen>() : nullptr;
    return cocoaScreen ? cocoaScreen->nativeScreen() : nil;
}

NSToolbar *makeToolbar()
{
    RCToolbarDelegate *delegate = [[RCToolbarDelegate alloc] init];
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"BilinToolbar"];
    toolbar.delegate = delegate;
    // NSToolbar 对 delegate 是弱引用，挂到工具栏上保证生命周期一致
    objc_setAssociatedObject(toolbar, @selector(delegate), delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return toolbar;
}

constexpr NSWindowButton kTitleButtons[] = {
    NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton
};

} // namespace

void WindowChrome::applyTitleBar(QWindow *window, const QString &style)
{
    NSWindow *nsWindow = nativeWindow(window);
    if (!nsWindow)
        return;

    if (!nsWindow.toolbar)
        nsWindow.toolbar = makeToolbar();
    nsWindow.toolbarStyle = style == QLatin1String("compact") ? NSWindowToolbarStyleUnifiedCompact
                                                              : NSWindowToolbarStyleUnified;
    nsWindow.titleVisibility = NSWindowTitleHidden;
    nsWindow.titlebarAppearsTransparent = YES;
}

void WindowChrome::setToolbarVisible(QWindow *window, bool visible)
{
    NSWindow *nsWindow = nativeWindow(window);
    if (nsWindow.toolbar)
        nsWindow.toolbar.visible = visible;
}

QRectF WindowChrome::titleButtonsRect(QWindow *window) const
{
    NSWindow *nsWindow = nativeWindow(window);
    if (!nsWindow)
        return {};

    NSRect bounds = NSZeroRect;
    for (NSWindowButton kind : kTitleButtons) {
        NSButton *button = [nsWindow standardWindowButton:kind];
        if (button)
            bounds = NSUnionRect(bounds, [button convertRect:button.bounds toView:nil]);
    }
    if (NSIsEmptyRect(bounds))
        return {};

    // AppKit 窗口坐标原点在左下角，换算成 Qt 的左上角坐标
    const CGFloat height = nsWindow.contentView.frame.size.height;
    return QRectF(bounds.origin.x, height - NSMaxY(bounds), bounds.size.width, bounds.size.height);
}

QRectF WindowChrome::screenNotch(QPointF point) const
{
    QScreen *screen = QGuiApplication::screenAt(point.toPoint());
    NSScreen *nsScreen = nativeScreen(screen);
    if (!nsScreen || nsScreen.safeAreaInsets.top <= 0)
        return {};
    // 刘海左右两边是菜单栏能用的区域，中间剩下的就是刘海
    const CGFloat left = nsScreen.auxiliaryTopLeftArea.size.width;
    const CGFloat right = nsScreen.auxiliaryTopRightArea.size.width;
    const CGFloat width = nsScreen.frame.size.width - left - right;
    if (width <= 0)
        return {};
    const QRect geometry = screen->geometry();
    return QRectF(geometry.x() + left, geometry.y(), width, nsScreen.safeAreaInsets.top);
}

qreal WindowChrome::menuBarHeight(QPointF point) const
{
    NSScreen *nsScreen = nativeScreen(QGuiApplication::screenAt(point.toPoint()));
    if (!nsScreen)
        return 0;
    // 可用区域不含菜单栏和程序坞，程序坞不会在顶上：上沿到屏幕顶端的那一段就是菜单栏
    const CGFloat height = NSMaxY(nsScreen.frame) - NSMaxY(nsScreen.visibleFrame);
    return height > 0 ? height : NSStatusBar.systemStatusBar.thickness;
}

void WindowChrome::placeOverMenuBar(QWindow *window)
{
    NSWindow *nsWindow = nativeWindow(window);
    NSScreen *screen = nsWindow.screen;
    if (!screen)
        return;
    nsWindow.level = NSMainMenuWindowLevel + 1;
    nsWindow.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary
                                | NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorIgnoresCycle;
    // 上沿贴回屏幕顶端；之后 Qt 改位置走 setFrame，系统不会再挪它
    NSRect frame = nsWindow.frame;
    frame.origin.y = NSMaxY(screen.frame) - frame.size.height;
    [nsWindow setFrame:frame display:YES];
}


void WindowChrome::setTitleBar(QQuickItem *)
{
}

void WindowChrome::setWindowButton(QQuickItem *, const QString &)
{
}

void WindowChrome::setInteractive(QQuickItem *)
{
}
