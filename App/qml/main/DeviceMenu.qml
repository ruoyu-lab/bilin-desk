import QtQuick

AppMenu {
    id: root

    // Devices 里的一项（device_directory.h）；还没选的时候是空的
    property var device: ({ favorite: false, saved: false })

    signal connectRequested(bool viewOnly)

    AppMenuItem { text: qsTr("连接"); onTriggered: root.connectRequested(false) }
    AppMenuItem { text: qsTr("仅查看"); onTriggered: root.connectRequested(true) }
    AppMenuSeparator {}
    AppMenuItem {
        text: root.device.favorite ? qsTr("取消收藏") : qsTr("收藏")
        onTriggered: Devices.toggleFavorite(root.device.key)
    }
    AppMenuItem {
        text: qsTr("复制地址")
        onTriggered: Clipboard.setText(root.device.address)
    }
    AppMenuSeparator {}
    AppMenuItem {
        text: root.device.saved ? qsTr("移除设备") : qsTr("保存设备")
        destructive: root.device.saved
        onTriggered: root.device.saved ? Devices.forget(root.device.key) : Devices.save(root.device.key)
    }
}
