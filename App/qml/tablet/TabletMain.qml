pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

// 平板（iPad、Android 平板）的窗口（main_mobile.cpp 按屏幕选，手机是 MobileMain）：和电脑版的主窗口一个样子，
// 左边侧栏，右边概览、设备列表、设置（这几页和电脑版共用，被别人连的那些平板上没有）。一个窗口铺满屏幕，
// 连接、登录一层层推进来（StackView），系统的返回键退回上一层
ApplicationWindow {
    id: app

    visible: true
    title: qsTr("比邻")
    color: Theme.bg
    // 铺到状态栏底下，各页按 SafeArea 让开
    flags: Qt.Window | Qt.ExpandedClientAreaHint

    property bool sidebarOpen: true
    // 手指怎么操作电脑：设置里选的；没选过时平板直接点
    readonly property string fingerMode: AppSettings.fingerMode.length > 0 ? AppSettings.fingerMode : "direct"

    function connectTo(device, viewOnly) {
        Devices.touch(device.key)
        stack.push(sessionPage, { device: device, viewOnly: viewOnly })
    }

    // 刚打开时等后台服务把账号的状态给过来，再决定要不要先进登录页（同 MobileMain）
    property bool loginDecided: false
    function decideLogin() {
        if (loginDecided || ThisDevice.serviceState !== "ready")
            return
        loginDecided = true
        if (!Account.signedIn && !AppSettings.mobileLoginSkipped)
            Account.loginRequested = true
    }

    Component.onCompleted: Qt.callLater(decideLogin)

    // 状态栏的字跟着程序的外观
    Binding {
        target: PhoneWindow
        property: "dark"
        value: Theme.dark
    }
    Connections {
        target: ThisDevice
        function onServiceStateChanged() { Qt.callLater(app.decideLogin) }
    }
    // 侧栏的账号、设置里点了登录：推出登录页；登录好了、选了以后再说：回去
    Connections {
        target: Account
        function onLoginRequestedChanged() {
            if (Account.loginRequested)
                stack.push(loginPage)
            else if (stack.currentItem instanceof MobileLogin)
                stack.pop()
        }
        function onChanged() {
            if (Account.signedIn)
                Account.loginRequested = false
        }
    }

    // 系统的返回（Android 平板）：退回上一层；连着电脑时先问要不要断开
    onClosing: close => {
        if (stack.depth <= 1)
            return
        close.accepted = false
        const session = stack.currentItem as MobileSession
        if (session)
            session.askDisconnect()
        else if (stack.currentItem instanceof MobileLogin)
            Account.loginRequested = false
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

        BrowseState { id: browse }

        Sidebar {
            id: sidebar
            x: app.sidebarOpen ? 0 : -width
            width: 240
            height: parent.height
            browse: browse
            topInset: home.SafeArea.margins.top

            Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }

        Item {
            anchors.fill: parent
            anchors.leftMargin: Math.max(0, sidebar.x + sidebar.width)
            anchors.topMargin: home.SafeArea.margins.top
            anchors.rightMargin: home.SafeArea.margins.right
            anchors.bottomMargin: home.SafeArea.margins.bottom

            MainToolbar {
                id: toolbar
                width: parent.width
                browse: browse
                onAddRequested: addDialog.open()
            }

            OverviewPage {
                anchors.top: toolbar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: browse.view === "home"
                onConnectRequested: (device, viewOnly) => app.connectTo(device, viewOnly)
                onDetailsRequested: (listView, key) => browse.showDevice(listView, key)
                onAddRequested: addDialog.open()
            }

            DevicesPage {
                anchors.top: toolbar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: ["mine", "nearby", "favorites", "recent"].includes(browse.view)
                browse: browse
                onConnectRequested: (device, viewOnly) => app.connectTo(device, viewOnly)
            }

            SettingsPage {
                anchors.top: toolbar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: browse.view === "settings"
                extraSections: Component {
                    RemoteControlSettings {
                        fingerMode: app.fingerMode
                        icons: true
                    }
                }
            }
        }

        // 侧栏开关：左上角，侧栏收起后位置不变
        IconButton {
            x: home.SafeArea.margins.left + 14
            y: home.SafeArea.margins.top + Math.round((Theme.headerHeight - height) / 2)
            iconName: "sidebar"
            text: app.sidebarOpen ? qsTr("隐藏侧边栏") : qsTr("显示侧边栏")
            onClicked: app.sidebarOpen = !app.sidebarOpen
        }

        AddDeviceDialog {
            id: addDialog
            onConnectRequested: device => app.connectTo(device, false)
        }
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
                Account.loginRequested = false
            }
        }
    }
}
