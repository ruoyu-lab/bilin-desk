import QtQuick

AppMenu {
    id: root

    // Devices 里的一项（device_directory.h）；还没选的时候是空的
    property var device: ({ favorite: false, saved: false })
    // 有设备详情的地方（电脑版、平板的概览）多一项"详情"
    property bool details: false

    signal connectRequested()
    signal detailsRequested()

    // 不在线的我的设备、本机叫得醒时（DeviceWake）：先叫醒它
    readonly property bool wakeable: {
        Wake.revision
        return device.mine === true && device.presenceKnown === true && !device.online && !!device.deviceId
               && Wake.problem(device.deviceId).length === 0
    }

    AppMenuItem { text: qsTr("连接"); onTriggered: root.connectRequested() }
    AppMenuItem {
        visible: root.wakeable
        height: visible ? implicitHeight : 0
        text: qsTr("唤醒")
        onTriggered: Wake.wake(root.device.deviceId)
    }
    AppMenuItem {
        // 菜单里藏起来的一项也占着地方：高度跟着清零
        visible: root.details
        height: visible ? implicitHeight : 0
        text: qsTr("详情")
        onTriggered: root.detailsRequested()
    }
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
