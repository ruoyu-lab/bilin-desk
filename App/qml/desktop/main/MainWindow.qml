import QtQuick
import QtQuick.Controls.Basic

// 主窗口。按需创建、关掉就销毁（Main.qml）；位置、大小记下来，下次打开还在那里（WindowMemory）。
// 去连接别人时发出 sessionRequested，会话窗口由 Main.qml 管：主窗口关了，会话照常
ApplicationWindow {
    id: window

    property bool sidebarOpen: true
    property int sidebarWidth: 216
    // 右边内容最窄多宽：窗口缩不到比它窄，侧栏也拖不过去（侧栏收着也算上它，打开时不用撑大窗口）
    readonly property int contentMinimumWidth: 564

    signal sessionRequested(var device, bool viewOnly)
    // 设置里点了"卸载远程控制"（macOS）
    signal uninstallRequested()

    width: 1080
    height: 700
    minimumWidth: sidebarWidth + contentMinimumWidth
    minimumHeight: 480
    title: qsTr("比邻")
    color: Theme.bg
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    readonly property WindowMemory memory: WindowMemory {
        window: window
        key: "main"
    }

    Component.onCompleted: {
        WindowChrome.applyTitleBar(window, "unified")
        WindowChrome.setInteractive(mainToolbar.tools)
        memory.restore()
    }
    function connect(device, viewOnly) {
        Devices.touch(device.key)
        window.sessionRequested(device, viewOnly)
    }

    BrowseState { id: browse }

    // Windows：顶部这一行是标题栏（拖动窗口、双击最大化）；可以点的东西另外登记
    Item {
        id: titleBar
        width: parent.width
        height: Theme.headerHeight
        Component.onCompleted: WindowChrome.setTitleBar(titleBar)
    }

    Sidebar {
        id: sidebar
        x: window.sidebarOpen ? 0 : -width
        width: window.sidebarWidth
        height: parent.height
        browse: browse

        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: Math.max(0, sidebar.x + sidebar.width)

        // macOS：统一工具栏那一行；Windows：标题栏那一行，工具排在窗口按钮左边
        MainToolbar {
            id: mainToolbar
            width: parent.width
            browse: browse
            rightInset: windowButtons.visible ? windowButtons.width + windowButtons.inset + windowButtons.spacing : 0
            // 组件里的 toolbar 是通知按钮自己的属性，外面的工具栏换个名字认
            homeTools: Component { NotificationsButton { toolbar: mainToolbar } }
            onAddRequested: addDialog.open()
        }

        OverviewPage {
            anchors.top: mainToolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: browse.view === "home"
            banner: Component { PermissionBanner {} }
            thisDevice: Component { ThisDeviceCard {} }
            onConnectRequested: (device, viewOnly) => window.connect(device, viewOnly)
            onDetailsRequested: (listView, key) => browse.showDevice(listView, key)
            onAddRequested: addDialog.open()
        }

        DevicesPage {
            anchors.top: mainToolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: ["mine", "nearby", "favorites", "recent"].includes(browse.view)
            browse: browse
            onConnectRequested: (device, viewOnly) => window.connect(device, viewOnly)
        }

        // macOS：设置从窗口顶端开始，顶上只留 30px 拖动窗口；Windows 在标题栏下面
        SettingsPage {
            anchors.top: Theme.unifiedTitleBar ? parent.top : mainToolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: browse.view === "settings"
            overTitleBar: Theme.unifiedTitleBar
            machineRows: Component { MachineSettingsRows {} }
            generalRows: Component { DownloadFolderRow {} }
            // 只有 macOS 上装好的那一份有（开发版本没有）
            uninstallable: Qt.platform.os === "osx" && Setup.installed
            // 侧栏收起时，红绿灯和侧栏开关压在页面左上角
            leftInset: Theme.unifiedTitleBar
                       ? Math.max(0, sidebarToggle.x + sidebarToggle.width + 12 - Math.max(0, sidebar.x + sidebar.width)) : 0
            onUninstallRequested: window.uninstallRequested()
        }
    }

    // 侧栏右边缘：无视觉变化，悬停显示左右箭头，拖动调整宽度
    MouseArea {
        property real pressX
        property int pressWidth

        x: sidebar.x + sidebar.width - 3
        width: 6
        height: parent.height
        visible: window.sidebarOpen
        cursorShape: Qt.SizeHorCursor
        preventStealing: true

        onPressed: mouse => {
            pressX = mapToItem(null, mouse.x, 0).x
            pressWidth = window.sidebarWidth
        }
        onPositionChanged: mouse => {
            const delta = mapToItem(null, mouse.x, 0).x - pressX
            const widest = Math.min(340, window.width - window.contentMinimumWidth)
            window.sidebarWidth = Math.round(Math.max(180, Math.min(widest, pressWidth + delta)))
        }
    }

    WindowButtons {
        id: windowButtons
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: windowButtons.inset
        height: Theme.headerHeight
        separated: browse.view !== "settings"
    }

    // Windows：标题栏左上角是软件名，右边紧跟侧栏开关（macOS 不显示软件名）
    Text {
        id: appName
        visible: !Theme.unifiedTitleBar
        x: 14
        height: Theme.headerHeight
        verticalAlignment: Text.AlignVCenter
        text: qsTr("比邻")
        font.pixelSize: 12
        font.weight: Font.Medium
        color: Theme.textMuted
    }

    // macOS 固定在红绿灯右侧；Windows 在软件名右边。侧栏收起后位置不变
    IconButton {
        id: sidebarToggle
        x: Theme.unifiedTitleBar ? 90 : appName.x + appName.width + 6
        y: Math.round((Theme.headerHeight - height) / 2)
        Component.onCompleted: WindowChrome.setInteractive(sidebarToggle)
        implicitWidth: Theme.unifiedTitleBar ? 28 : Theme.titleButtonSize
        implicitHeight: Theme.unifiedTitleBar ? 28 : Theme.titleButtonSize
        iconSize: Theme.titleIconSize
        iconName: "sidebar"
        text: window.sidebarOpen ? qsTr("隐藏侧边栏") : qsTr("显示侧边栏")
        onClicked: window.sidebarOpen = !window.sidebarOpen
    }

    AddDeviceDialog {
        id: addDialog
        onConnectRequested: device => window.connect(device, false)
    }

}
