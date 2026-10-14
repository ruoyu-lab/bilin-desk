import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 会话工具按钮：对方的显示器（有几块时） / 控制与仅查看（一个按钮切换） / 快捷键 / 全屏 / 侧边面板 /
// 电源（断开连接，或者让对方电脑重启、关机、睡眠、注销）
RowLayout {
    id: root

    required property ApplicationWindow session

    readonly property var view: session.currentView
    readonly property bool viewOnly: view !== null && view.viewOnly

    spacing: 2

    DisplayPicker {
        id: displayPicker
        session: root.view !== null ? root.view.session : null
    }
    Rectangle {
        visible: displayPicker.visible
        Layout.alignment: Qt.AlignCenter
        Layout.margins: 4
        Layout.preferredWidth: 1
        Layout.preferredHeight: 16
        color: Theme.border
    }

    IconButton {
        compact: !Theme.unifiedTitleBar
        id: modeToggle
        enabled: root.view !== null
        iconName: root.viewOnly ? "eye" : "pointer"
        text: root.viewOnly ? qsTr("仅查看，点击切换为控制") : qsTr("控制中，点击切换为仅查看")
        active: root.viewOnly
        onClicked: root.session.setViewOnly(!root.viewOnly)
    }
    IconButton {
        compact: !Theme.unifiedTitleBar
        id: keys
        iconName: "keyboard"
        text: qsTr("发送快捷键")
        active: root.session.keysMenuOpen
        onClicked: root.session.showKeysMenu(keys)
    }
    IconButton {
        compact: !Theme.unifiedTitleBar
        iconName: root.session.fullScreen ? "minimize" : "maximize"
        text: root.session.fullScreen ? qsTr("退出全屏") : qsTr("全屏")
        onClicked: root.session.toggleFullScreen()
    }

    Rectangle {
        Layout.alignment: Qt.AlignCenter
        Layout.margins: 4
        Layout.preferredWidth: 1
        Layout.preferredHeight: 16
        color: Theme.border
    }

    IconButton {
        compact: !Theme.unifiedTitleBar
        iconName: "panel-right"
        text: root.session.panelOpen ? qsTr("收起面板") : qsTr("对话、文件与设置")
        onClicked: root.session.panelOpen = !root.session.panelOpen

        // 面板收着时对方说了话
        Rectangle {
            x: parent.width - width - 4
            y: 4
            width: 7
            height: 7
            radius: 3.5
            color: Theme.danger
            visible: root.view !== null && !root.view.panelOpen && root.view.session !== null
                     && root.view.session.chat.conversation.unread > 0
        }
    }
    IconButton {
        compact: !Theme.unifiedTitleBar
        id: power
        iconName: "power"
        text: qsTr("断开连接、重启或关机对方电脑")
        danger: true
        active: root.session.powerMenuOpen
        onClicked: root.session.showPowerMenu(power)
    }
}
