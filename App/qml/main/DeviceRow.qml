import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var device
    property bool selected: false

    signal selectRequested
    signal connectRequested(bool viewOnly)

    implicitHeight: 50

    HoverHandler { id: hover }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: root.selected ? Theme.selection : hover.hovered ? Theme.hover : "transparent"
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            root.selectRequested()
            if (mouse.button === Qt.RightButton)
                menu.popup()
        }
        onDoubleClicked: root.connectRequested(false)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 10

        DeviceIcon {
            os: root.device.os
            size: 30
            opacity: root.device.presenceKnown && !root.device.online ? 0.5 : 1
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: root.device.name
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                text: root.device.address
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Theme.textSecondary
            }
        }

        Icon {
            visible: root.device.favorite
            name: "star-filled"
            size: 12
            color: Theme.favorite
        }

        // 只有知道在不在线时（附近的）才有这个点
        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            visible: root.device.presenceKnown
            radius: 3.5
            color: root.device.online ? Theme.success : Theme.textMuted
        }
    }

    DeviceMenu {
        id: menu
        device: root.device
        onConnectRequested: viewOnly => root.connectRequested(viewOnly)
    }
}
