import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 概览页的设备块：stacked 为竖排（收藏），否则横排（附近）
Rectangle {
    id: root

    required property var device
    property bool stacked: false
    property string subtitle

    signal detailsRequested
    signal connectRequested(bool viewOnly)

    radius: 10
    color: hover.hovered ? Theme.cardHover : Theme.card

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton)
                menu.popup()
            else
                root.detailsRequested()
        }
        onDoubleClicked: root.connectRequested(false)
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
            Layout.rightMargin: root.stacked ? 0 : connect.width
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

    AbstractButton {
        id: connect
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: root.stacked ? undefined : parent.verticalCenter
        anchors.top: root.stacked ? parent.top : undefined
        anchors.topMargin: 10
        implicitWidth: 48
        implicitHeight: 26
        opacity: hover.hovered ? 1 : 0
        enabled: opacity > 0
        onClicked: root.connectRequested(false)

        Behavior on opacity { NumberAnimation { duration: 120 } }

        background: Rectangle {
            radius: 13
            color: connect.down ? Theme.accentPressed : connect.hovered ? Theme.accentHover : Theme.accent
        }
        contentItem: Text {
            text: qsTr("连接")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 12
            color: Theme.accentText
        }
    }

    DeviceMenu {
        id: menu
        device: root.device
        onConnectRequested: viewOnly => root.connectRequested(viewOnly)
    }
}
