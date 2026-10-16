import QtQuick
import QtQuick.Layouts

// 手机上一台设备的详情（按住设备出的菜单里点"详情"）：上面返回，下面和电脑版、平板一样（DeviceDetail，放不下时滚动）。
// 按 key 认：设备列表一变（改名、备注、收到它桌面的背景）跟着换成新的
Item {
    id: root

    required property string key
    readonly property var device: {
        Devices.devices
        return Devices.find(key)
    }

    signal backRequested()
    signal connectRequested(var device, bool viewOnly)

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    RowLayout {
        id: header
        x: root.SafeArea.margins.left + 8
        y: root.SafeArea.margins.top + 6
        width: parent.width - x - root.SafeArea.margins.right - 8
        spacing: 4
        IconButton {
            iconName: "chevron-right"
            rotation: 180
            text: qsTr("返回")
            onClicked: root.backRequested()
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("设备详情")
            elide: Text.ElideRight
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: Theme.text
        }
    }

    // 设备被移除了（不在列表里了）：只剩返回
    Loader {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.SafeArea.margins.left
        anchors.rightMargin: root.SafeArea.margins.right
        anchors.bottomMargin: root.SafeArea.margins.bottom
        active: root.device.key !== undefined
        sourceComponent: DeviceDetail {
            device: root.device
            onConnectRequested: viewOnly => root.connectRequested(root.device, viewOnly)
        }
    }
}
