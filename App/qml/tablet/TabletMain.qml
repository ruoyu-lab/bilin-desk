pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

// 平板（iPad、Android 平板）的窗口（main_mobile.cpp 按屏幕选，手机是 MobileMain）：和电脑版的主窗口一个样子，
// 左边侧栏，右边概览、设备列表、设置（这几页和电脑版共用，被别人连的那些平板上没有）。连上电脑是电脑版的会话
// （TabletSession，盖在主界面上，回到主界面时连接不断）。一个窗口铺满屏幕，登录页推进来（StackView）
ApplicationWindow {
    id: app

    title: qsTr("比邻")
    color: Theme.bg
    // 全屏：不要状态栏、Android 的导航条（iPad 底部的横条不碰时隐去，PhoneWindow）。圆角、刘海按 SafeArea 让开
    flags: Qt.Window | Qt.ExpandedClientAreaHint
    // 电脑上预览平板的界面（BILIN_PREVIEW=tablet）时是普通窗口
    visibility: Theme.touch ? Window.FullScreen : Window.Windowed
    onActiveChanged: {
        if (active)
            PhoneWindow.hideHomeIndicator()
    }
    // 窗口不替内容让开 SafeArea（Qt 默认让，底下横条那里会空着一条）：侧栏、会话铺到底，各自按 SafeArea 让开
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    property bool sidebarOpen: true
    // 正在看会话（不是主界面）
    property bool showingSessions: false
    // 手指怎么操作电脑：设置里选的；没选过时平板直接点
    readonly property string fingerMode: AppSettings.fingerMode.length > 0 ? AppSettings.fingerMode : "direct"

    function connectTo(device, viewOnly) {
        Devices.touch(device.key)
        sessions.open(device, viewOnly)
        showingSessions = true
    }

    // 刚打开时等服务器说了登没登录，再决定要不要先进登录页（同 MobileMain）
    property bool loginDecided: false
    function decideLogin() {
        if (loginDecided || ThisDevice.serviceState !== "ready" || !Account.known)
            return
        loginDecided = true
        if (!Account.signedIn && !AppSettings.mobileLoginSkipped)
            Account.loginRequested = true
    }

    Component.onCompleted: Qt.callLater(decideLogin)

    // 按钮、字用电脑版的大小（不是手机的）
    Binding {
        target: Theme
        property: "tablet"
        value: true
    }
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
            Qt.callLater(app.decideLogin)
            if (Account.signedIn)
                Account.loginRequested = false
        }
    }

    // 系统的返回（Android 平板）：登录页、会话回到主界面（连接不断）
    onClosing: close => {
        if (stack.depth > 1) {
            close.accepted = false
            Account.loginRequested = false
        } else if (showingSessions) {
            close.accepted = false
            showingSessions = false
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: main
    }

    Item {
        id: main

        Item {
            id: home
            anchors.fill: parent
            visible: !app.showingSessions

            BrowseState { id: browse }

            Sidebar {
                id: sidebar
                x: app.sidebarOpen ? 0 : -width
                width: 240
                height: parent.height
                browse: browse
                topInset: home.SafeArea.margins.top
                bottomInset: home.SafeArea.margins.bottom
                sessionCount: sessions.count
                onSessionsRequested: app.showingSessions = true

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

        TabletSession {
            id: sessions
            anchors.fill: parent
            visible: app.showingSessions
            fingerMode: app.fingerMode
            onBackRequested: app.showingSessions = false
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
