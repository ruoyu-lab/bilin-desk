import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 概览页的设备块：stacked 为竖排（收藏），否则横排（附近）。点一下就连接，详情在菜单里
Rectangle {
    id: root

    required property var device
    property bool stacked: false
    property string subtitle

    signal detailsRequested
    signal connectRequested(bool viewOnly)

    radius: 10
    color: hover.hovered || tap.pressed ? Theme.cardHover : Theme.card

    HoverHandler {
        id: hover
        enabled: Theme.hoverEffects
        cursorShape: Qt.PointingHandCursor
    }

    // 点一下连接；右键（触屏上长按）出菜单：详情、收藏……（只看在会话的工具栏上切换）
    TapHandler {
        id: tap
        onTapped: root.connectRequested(false)
        onLongPressed: menu.popup(root, point.position.x, point.position.y)
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onTapped: (point, button) => menu.popup(root, point.position.x, point.position.y)
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 12
        columns: root.stacked ? 1 : 2
        columnSpacing: 10
        rowSpacing: 8

        DeviceIcon {
            Layout.alignment: root.stacked ? Qt.AlignLeft | Qt.AlignTop : Qt.AlignVCenter
            os: root.device.os
            size: root.stacked ? 30 : 32
            opacity: root.device.presenceKnown && !root.device.online ? 0.5 : 1
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: root.stacked ? Qt.AlignBottom : Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.device.name
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.text
            }
            RowLayout {
                spacing: 5
                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    visible: root.device.presenceKnown
                    radius: 3
                    color: root.device.online ? Theme.success : Theme.textMuted
                }
                Text {
                    Layout.fillWidth: true
                    text: root.subtitle
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }
        }
    }

    DeviceMenu {
        id: menu
        device: root.device
        details: true
        onConnectRequested: root.connectRequested(false)
        onDetailsRequested: root.detailsRequested()
    }
}
