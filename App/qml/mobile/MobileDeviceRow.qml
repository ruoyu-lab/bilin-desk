import QtQuick
import QtQuick.Layouts

// 设备列表里的一行（放在卡片里，一行接一行）：系统图标、名字、在不在线和设备 ID（或别的说明）；
// 点一下连接，按住不放出菜单
Item {
    id: root

    // Devices 里的一项（device_directory.h）
    required property var device
    // 名字下面那行；空的时候：知道在不在线的写"在线 · ID"，不知道的只写 ID 或 IP
    property string detail
    // 卡片里的第一行上面没有分隔线
    property bool first: false

    signal clicked()
    signal pressAndHold()

    readonly property string subtitle: detail.length > 0 ? detail
        : device.presenceKnown ? (device.online ? qsTr("在线") : qsTr("离线")) + " · " + device.address
        : device.address

    implicitHeight: 64

    Rectangle {
        anchors.fill: parent
        color: tap.pressed ? Theme.hover : "transparent"
    }

    Rectangle {
        visible: !root.first
        x: 64
        width: parent.width - x
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        spacing: 12

        DeviceIcon {
            os: root.device.os
            size: 38
            opacity: root.device.presenceKnown && !root.device.online ? 0.5 : 1
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                Layout.fillWidth: true
                text: root.device.name
                elide: Text.ElideRight
                font.pixelSize: 16
                color: Theme.text
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 5
                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    visible: root.device.presenceKnown
                    radius: 3.5
                    color: root.device.online ? Theme.success : Theme.textMuted
                }
                Text {
                    Layout.fillWidth: true
                    text: root.subtitle
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }
            }
        }
        Icon {
            visible: root.device.favorite
            name: "star-filled"
            size: 16
            color: Theme.favorite
        }
        Icon {
            name: "chevron-right"
            size: 16
            color: Theme.textMuted
        }
    }

    TapHandler {
        id: tap
        onTapped: root.clicked()
        onLongPressed: root.pressAndHold()
    }
}
