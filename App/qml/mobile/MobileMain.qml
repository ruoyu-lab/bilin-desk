pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 手机的窗口（main_mobile.cpp；平板是 TabletMain）：一个窗口铺满屏幕，四个标签页（概览、设备、最近、设置）。
// 窄的时候标签在底部，宽的时候（横着）在左边的侧栏。连接、账号、关于这些一层层推进来（StackView），
// 系统的返回键退回上一层。没登录、也没选过"以后再说"时先到登录页
ApplicationWindow {
    id: app

    visible: true
    title: qsTr("比邻")
    color: Theme.bg
    // 铺到状态栏、导航条底下，各页按 SafeArea 让开
    flags: Qt.Window | Qt.ExpandedClientAreaHint

    readonly property bool wide: width >= 720
    // 手指怎么操作电脑：设置里选的；没选过时手机是触摸板
    readonly property string fingerMode: AppSettings.fingerMode.length > 0 ? AppSettings.fingerMode : "touchpad"
    property int currentTab: 0

    function connectTo(device, viewOnly) {
        stack.push(sessionPage, { device: device, viewOnly: viewOnly })
    }

    function showMenu(device) {
        deviceMenu.device = device
        deviceMenu.open()
    }

    // 刚打开时等后台服务把账号的状态给过来（服务信息先到、账号随后），再决定要不要先进登录页
    property bool loginDecided: false
    function decideLogin() {
        if (loginDecided || ThisDevice.serviceState !== "ready")
            return
        loginDecided = true
        if (!Account.signedIn && !AppSettings.mobileLoginSkipped)
            stack.push(loginPage, {}, StackView.Immediate)
    }

    Component.onCompleted: Qt.callLater(decideLogin)

    // 状态栏、导航条的图标跟着程序的外观（程序选了深色、手机是浅色时也看得清）
    Binding {
        target: PhoneWindow
        property: "dark"
        value: Theme.dark
    }
    Connections {
        target: ThisDevice
        function onServiceStateChanged() { Qt.callLater(app.decideLogin) }
    }
    // 在登录页上登录好了：回去；在账号页上退出了：回去
    Connections {
        target: Account
        function onChanged() {
            if (Account.signedIn && stack.currentItem instanceof MobileLogin)
                stack.pop()
            // 在账号页上退出、注销了
            else if (!Account.signedIn && stack.currentItem instanceof AccountSettings)
                stack.pop()
        }
    }

    // 系统的返回：退回上一层；连着电脑时先问要不要断开
    onClosing: close => {
        if (stack.depth <= 1)
            return
        close.accepted = false
        const session = stack.currentItem as MobileSession
        if (session)
            session.askDisconnect()
        else
            stack.pop()
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: home
    }

    Item {
        id: home

        readonly property real tabInset: app.wide ? 0 : tabBar.height

        MobileNavRail {
            id: rail
            visible: app.wide
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            currentIndex: app.currentTab
            onSelected: index => app.currentTab = index
        }

        StackLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: app.wide ? rail.right : parent.left
            anchors.right: parent.right
            currentIndex: app.currentTab

            MobileOverview {
                bottomInset: home.tabInset
                onConnectRequested: (device, viewOnly) => app.connectTo(device, viewOnly)
                onMenuRequested: device => app.showMenu(device)
                onAllDevicesRequested: app.currentTab = 1
                onRecentRequested: app.currentTab = 2
                onLoginRequested: stack.push(loginPage)
            }
            MobileDevices {
                bottomInset: home.tabInset
                onConnectRequested: (device, viewOnly) => app.connectTo(device, viewOnly)
                onMenuRequested: device => app.showMenu(device)
            }
            MobileRecent {
                bottomInset: home.tabInset
                onConnectRequested: (device, viewOnly) => app.connectTo(device, viewOnly)
                onMenuRequested: device => app.showMenu(device)
            }
            MobileSettings {
                bottomInset: home.tabInset
                fingerMode: app.fingerMode
                onAccountRequested: stack.push(accountPage)
                onLoginRequested: stack.push(loginPage)
                onAboutRequested: stack.push(aboutPage)
            }
        }

        MobileTabBar {
            id: tabBar
            visible: !app.wide
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            currentIndex: app.currentTab
            onSelected: index => app.currentTab = index
        }
    }

    // 按住一台设备不放：连接、仅查看、收藏、复制地址、移除
    DeviceMenu {
        id: deviceMenu
        parent: Overlay.overlay
        x: Math.round((app.width - width) / 2)
        y: Math.round((app.height - height) / 2)
        onConnectRequested: viewOnly => app.connectTo(deviceMenu.device, viewOnly)
    }

    Component {
        id: sessionPage
        MobileSession {
            fingerMode: app.fingerMode
            onClosed: stack.pop()
        }
    }

    Component {
        id: loginPage
        MobileLogin {
            onSkipRequested: {
                AppSettings.mobileLoginSkipped = true
                stack.pop()
            }
        }
    }

    Component {
        id: accountPage
        AccountSettings {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: aboutPage
        AboutSettings {
            onBackRequested: stack.pop()
            onDocumentRequested: kind => {
                if (kind === "licenses")
                    stack.push(licensesPage)
                else
                    stack.push(legalPage, { kind: kind })
            }
        }
    }

    Component {
        id: legalPage
        LegalPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: licensesPage
        LicensesPage {
            onBackRequested: stack.pop()
        }
    }
}
