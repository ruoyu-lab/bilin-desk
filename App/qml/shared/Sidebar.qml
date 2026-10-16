import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    required property BrowseState browse
    // 顶上、底下多让出的高度（平板：状态栏、iPad 底部的横条）
    property real topInset: 0
    property real bottomInset: 0
    // 平板：回到主界面时还连着几台，有的话最上面一项回到会话
    property int sessionCount: 0

    signal sessionsRequested()
    readonly property string accountName: Account.signedIn ? Account.displayName : qsTr("未登录")

    color: Theme.sidebar

    // 顶部是红绿灯和侧栏开关所在的区域，用来拖动窗口
    WindowDragArea {
        width: parent.width
        height: column.anchors.topMargin
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 10
        anchors.bottomMargin: 10 + root.bottomInset
        // macOS 紧挨着红绿灯那一行；Windows 在标题栏（软件名、侧栏开关）下面
        anchors.topMargin: (Theme.unifiedTitleBar ? Theme.headerHeight - 4 : Theme.headerHeight + 3) + root.topInset
        spacing: 2

        NavItem {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            visible: root.sessionCount > 0
            iconName: "pointer"
            text: qsTr("正在连接 · %1").arg(root.sessionCount)
            onClicked: root.sessionsRequested()
        }
        NavItem {
            Layout.fillWidth: true
            iconName: "home"
            text: qsTr("概览")
            current: root.browse.view === "home"
            onClicked: root.browse.view = "home"
        }
        NavItem {
            Layout.fillWidth: true
            iconName: "monitor"
            text: qsTr("我的设备")
            current: root.browse.view === "mine"
            onClicked: root.browse.view = "mine"
        }
        NavItem {
            Layout.fillWidth: true
            iconName: "radar"
            text: qsTr("附近")
            current: root.browse.view === "nearby"
            onClicked: root.browse.view = "nearby"
        }
        NavItem {
            Layout.fillWidth: true
            iconName: "star"
            text: qsTr("收藏")
            current: root.browse.view === "favorites"
            onClicked: root.browse.view = "favorites"
        }
        NavItem {
            Layout.fillWidth: true
            iconName: "clock"
            text: qsTr("最近")
            current: root.browse.view === "recent"
            onClicked: root.browse.view = "recent"
        }

        Item { Layout.fillHeight: true }

        // 有新版本时（只有装好的程序有）：点了下载、安装。装的时候本程序会被关掉，装完自己再打开
        AbstractButton {
            id: update
            readonly property string phase: ThisDevice.updatePhase
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            visible: ThisDevice.updateAvailable.length > 0
            enabled: phase === "idle"
            implicitHeight: 40
            hoverEnabled: true
            onClicked: ThisDevice.installUpdate()

            background: Rectangle {
                radius: Theme.radius
                color: update.hovered && update.enabled ? Theme.hover : Theme.selected

                // 下载的进度
                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 10
                    anchors.bottomMargin: 5
                    visible: update.phase === "downloading"
                    width: (parent.width - 20) * ThisDevice.updateProgress
                    height: 2
                    radius: 1
                    color: Theme.text
                }
            }
            contentItem: RowLayout {
                spacing: 10
                Item { Layout.preferredWidth: 2 }
                Icon {
                    name: "download"
                    size: 17
                    color: Theme.text
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: update.phase === "downloading"
                              ? qsTr("正在下载 %1%").arg(Math.floor(ThisDevice.updateProgress * 100))
                              : update.phase === "installing" ? qsTr("正在安装，装完自动打开")
                              : qsTr("更新到 %1").arg(ThisDevice.updateAvailable)
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: update.phase === "idle" && ThisDevice.updateFailed.length > 0
                        text: ThisDevice.updateFailed === "download" ? qsTr("上次没下载成，点一下再试")
                                                                    : qsTr("上次没装上，点一下再试")
                        elide: Text.ElideRight
                        font.pixelSize: 11
                        color: Theme.danger
                    }
                }
            }
        }

        RowLayout {
            spacing: 2

            AbstractButton {
                id: account
                Layout.fillWidth: true
                implicitHeight: 40
                hoverEnabled: true
                onClicked: accountPopup.open()

                background: Rectangle {
                    radius: Theme.radius
                    color: account.hovered || accountPopup.visible ? Theme.hover : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 9
                    Item { Layout.preferredWidth: 0 }
                    Avatar { name: root.accountName; size: 26 }
                    Text {
                        Layout.fillWidth: true
                        text: root.accountName
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: Theme.text
                    }
                }
            }

            IconButton {
                iconName: "settings"
                text: qsTr("设置")
                active: root.browse.view === "settings"
                onClicked: root.browse.view = "settings"
            }
        }
    }

    AccountPopup {
        id: accountPopup
        parent: account
        y: -height - 8
        accountName: root.accountName
    }
}
