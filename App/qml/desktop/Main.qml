import QtQuick
import Qt.labs.platform as Platform

// 程序的入口。窗口都按需创建、关掉就销毁：没有窗口时只剩托盘（Windows）/ 菜单栏（macOS）图标，
// 内存和显存还给系统（macOS 上 Dock 里也不占位置）。
// 启动时打开主窗口（macOS 第一次打开时先是授权引导）；登录时由系统启动的、后台服务拉起来的（有人连着本机时
// 界面没了）只在托盘 / 菜单栏
QtObject {
    id: app

    property LoginWindow loginWindow: null
    property MainWindow mainWindow: null
    property var sessionWindows: []
    // 卸载的界面（macOS：设置里点"卸载远程控制"）
    property SetupWindow setupWindow: null
    // macOS 第一次打开时的授权引导。没走完就关掉的，这次不再出现，下次打开还有
    property PermissionWindow permissionWindow: null
    property bool permissionGuideClosed: false
    readonly property bool permissionGuidePending: Qt.platform.os === "osx" && !AppSettings.permissionGuideDone
                                                   && !permissionGuideClosed
    // 被控时的刘海、连接请求、对话窗口：有人请求连接或连着本机时才有，之后过一会儿销毁
    property IncomingWindow incoming: null
    readonly property bool sharingActive: IncomingConnections.requests.length > 0 || IncomingConnections.viewers.length > 0
    readonly property bool hasWindows: loginWindow !== null || mainWindow !== null || sessionWindows.length > 0
                                       || setupWindow !== null || permissionWindow !== null

    readonly property Component loginComponent: Component { LoginWindow {} }
    readonly property Component mainComponent: Component { MainWindow {} }
    readonly property Component sessionComponent: Component { SessionWindow {} }
    readonly property Component incomingComponent: Component { IncomingWindow {} }
    readonly property Component setupComponent: Component { SetupWindow {} }
    readonly property Component permissionComponent: Component { PermissionWindow {} }

    function present(window) {
        if (window.visibility === Window.Minimized)
            window.showNormal()
        else
            window.show()
        window.raise()
        window.requestActivate()
    }

    // 把窗口拿出来：主窗口（不登录也能用）；正在登录时是登录窗口；macOS 第一次打开时先是授权引导
    function showWindow() {
        if (permissionGuidePending)
            present(ensurePermission())
        else
            present(Account.loginRequested ? ensureLogin() : ensureMain())
    }

    // 引导走完了、被关掉了：接着打开主窗口
    function ensurePermission() {
        if (!permissionWindow) {
            permissionWindow = permissionComponent.createObject(null)
            permissionWindow.finished.connect(() => app.permissionWindow.close())
            permissionWindow.closing.connect(() => Qt.callLater(() => {
                app.permissionGuideClosed = true
                app.permissionWindow.destroy()
                app.permissionWindow = null
                app.showWindow()
            }))
        }
        return permissionWindow
    }

    function ensureLogin() {
        if (!loginWindow) {
            loginWindow = loginComponent.createObject(null)
            loginWindow.closing.connect(() => Qt.callLater(discardLogin))
        }
        return loginWindow
    }

    function discardLogin() {
        if (!loginWindow)
            return
        loginWindow.destroy()
        loginWindow = null
    }

    function ensureMain() {
        if (!mainWindow) {
            mainWindow = mainComponent.createObject(null)
            mainWindow.sessionRequested.connect(openSession)
            mainWindow.uninstallRequested.connect(showUninstall)
            mainWindow.closing.connect(() => Qt.callLater(discardMain))
        }
        return mainWindow
    }

    function discardMain() {
        if (!mainWindow || mainWindow.visible)
            return
        mainWindow.destroy()
        mainWindow = null
    }

    // 会话窗口：新连接进入最近的那个；标签拖出来成为新窗口
    function createSessionWindow() {
        const created = sessionComponent.createObject(null)
        created.detachRequested.connect(index => createSessionWindow().receive(created.takeSession(index)))
        created.closing.connect(() => Qt.callLater(() => discardSession(created)))
        sessionWindows = sessionWindows.concat([created])
        return created
    }

    function discardSession(closed) {
        sessionWindows = sessionWindows.filter(w => w !== closed)
        closed.destroy()
    }

    function openSession(device, viewOnly) {
        const target = sessionWindows.length > 0 ? sessionWindows[sessionWindows.length - 1] : createSessionWindow()
        target.open(device, viewOnly)
    }

    // 卸载完了关掉它就退出（程序已经删了）
    function showUninstall() {
        discardSetup()
        const created = setupComponent.createObject(null)
        created.closing.connect(() => Qt.callLater(() => {
            if (Setup.status === "done") {
                AppLifecycle.quit()
                return
            }
            if (app.setupWindow === created && !created.visible)
                app.discardSetup()
        }))
        setupWindow = created
        present(created)
    }

    function discardSetup() {
        if (!setupWindow)
            return
        setupWindow.destroy()
        setupWindow = null
    }

    function updateIncoming() {
        if (sharingActive) {
            incomingRelease.stop()
            if (!incoming)
                incoming = incomingComponent.createObject(app)
        } else if (incoming) {
            incomingRelease.restart()
        }
    }

    onSharingActiveChanged: updateIncoming()
    onHasWindowsChanged: AppLifecycle.setDockVisible(hasWindows)

    // 窗口等启动完再说要不要拿出来（下面的 Autostart.launched）
    Component.onCompleted: updateIncoming()

    // 刘海收起来的动画、对话窗口关掉之后再销毁
    readonly property Timer incomingRelease: Timer {
        interval: 2000
        onTriggered: {
            if (app.sharingActive || !app.incoming)
                return
            app.incoming.destroy()
            app.incoming = null
        }
    }

    readonly property Connections lifecycle: Connections {
        target: AppLifecycle
        function onShowRequested() { app.showWindow() }
        // 有人连着本机时要退出（macOS 的 Cmd+Q、Dock 里的"退出"）：先问。有人连着时刘海那一套窗口一定在
        function onQuitHeld() { app.incoming.confirmQuit(false) }
    }

    // 启动完：登录时由系统启动的只在托盘 / 菜单栏（macOS 的 Dock 里也不占位置），别的时候把窗口拿出来
    readonly property Connections autostart: Connections {
        target: Autostart
        function onLaunched(atLogin) {
            if (atLogin || AppLifecycle.background)
                AppLifecycle.setDockVisible(false)
            else
                app.showWindow()
        }
    }

    readonly property Connections account: Connections {
        target: Account
        // 账号弹窗里点了登录 / 登录窗口关了、登录成功了
        function onLoginRequestedChanged() {
            if (Account.loginRequested)
                app.present(app.ensureLogin())
            else
                Qt.callLater(app.discardLogin)
        }
    }

    readonly property Platform.SystemTrayIcon tray: Platform.SystemTrayIcon {
        visible: true
        // macOS 菜单栏用单色模板图标，随系统的深浅色变化。
        // 托盘的图标加载器不按本文件的位置解析相对路径，也不认 @2x：给完整地址，每个平台一张 PNG，
        // 由系统缩放（macOS 32 像素按 2 倍图显示成 16 点；Windows 64 像素，缩到托盘需要的大小）
        icon.source: Qt.resolvedUrl(Qt.platform.os === "osx" ? "../icons/tray-mac.png" : "../icons/tray.png")
        icon.mask: Qt.platform.os === "osx"
        tooltip: qsTr("比邻")
        Component.onCompleted: if (!available) console.warn("系统托盘不可用")
        // Windows：左键单击打开窗口，右键是菜单；macOS 点一下就是菜单
        onActivated: reason => {
            if (reason === Platform.SystemTrayIcon.Trigger && Qt.platform.os !== "osx")
                app.showWindow()
        }

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("打开比邻")
                onTriggered: app.showWindow()
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                enabled: false
                text: qsTr("本机 ID  %1").arg(ThisDevice.deviceId)
            }
            Platform.MenuItem {
                enabled: false
                visible: ThisDevice.serviceState === "ready" && ThisDevice.acceptIncoming && IncomingConnections.accepting
                text: qsTr("临时密码  %1").arg(ThisDevice.password)
            }
            // 谁连着本机
            Platform.MenuItem {
                enabled: false
                visible: IncomingConnections.viewers.length > 0
                text: qsTr("正在连着  %1").arg(IncomingConnections.viewers.map(v => v.name).join("、"))
            }
            Platform.MenuItem {
                id: allowItem
                checkable: true
                enabled: ThisDevice.serviceState === "ready"
                checked: ThisDevice.acceptIncoming
                text: qsTr("允许别人连接")
                onTriggered: {
                    ThisDevice.acceptIncoming = allowItem.checked
                    allowItem.checked = Qt.binding(() => ThisDevice.acceptIncoming)
                }
            }
            Platform.MenuSeparator {}
            // 完全退出：后台服务也退出，本机不再能被连接。有人连着本机时先问
            Platform.MenuItem {
                text: qsTr("退出")
                onTriggered: {
                    if (IncomingConnections.viewers.length > 0) {
                        app.incoming.confirmQuit(true)
                        return
                    }
                    ThisDevice.quitService()
                    AppLifecycle.quit()
                }
            }
        }
    }
}
