pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../shared/device_query.js" as DeviceQuery
import "mobile_format.js" as MobileFormat

// 手机的概览：最上面连接到设备（输设备 ID、IP 或名称），下面我的设备（在线的在前，最多 4 台）和最近连过的
MobilePage {
    id: root

    title: qsTr("概览")

    property bool viewOnly: false

    signal connectRequested(var device, bool viewOnly)
    signal menuRequested(var device)
    signal allDevicesRequested()
    signal recentRequested()
    signal loginRequested()

    readonly property var mine: DeviceQuery.query(Devices.devices, "mine", { sort: "online" })
    readonly property var recent: DeviceQuery.query(Devices.devices, "recent", {})

    function connectFromInput() {
        const value = address.text.trim()
        if (value.length === 0) {
            error.text = qsTr("请输入设备 ID 或地址")
            error.visible = true
            address.forceActiveFocus()
            return
        }
        const device = Devices.ensure(value, "")
        if (!device.key) {
            error.text = qsTr("要填 9 位设备 ID，或者同一网络里的 IP 地址")
            error.visible = true
            address.forceActiveFocus()
            return
        }
        address.focus = false
        root.connectRequested(device, root.viewOnly)
        address.clear()
    }

    // ---- 新版本（Android 上从官网装的才有）：点了下载，交给系统的安装界面 ----
    Rectangle {
        id: update
        readonly property string phase: ThisDevice.updatePhase
        Layout.fillWidth: true
        visible: ThisDevice.updateAvailable.length > 0
        implicitHeight: updateRow.implicitHeight + 28
        radius: Theme.cardRadius
        color: Theme.card
        clip: true

        RowLayout {
            id: updateRow
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 14
            spacing: 12
            Icon {
                name: "download"
                size: 20
                color: Theme.text
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: qsTr("有新版本 %1").arg(ThisDevice.updateAvailable)
                    elide: Text.ElideRight
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    text: update.phase === "downloading"
                          ? qsTr("正在下载 %1%").arg(Math.floor(ThisDevice.updateProgress * 100))
                          : update.phase === "installing" ? qsTr("正在安装")
                          : ThisDevice.updateFailed === "download" ? qsTr("上次没下载成，再试一次")
                          : ThisDevice.updateFailed === "install" ? qsTr("上次没装上，再试一次")
                          : qsTr("现在是 %1").arg(Qt.application.version)
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    color: ThisDevice.updateFailed.length > 0 && update.phase === "idle" ? Theme.danger
                                                                                        : Theme.textSecondary
                }
            }
            AppButton {
                dark: true
                enabled: update.phase === "idle"
                text: qsTr("更新")
                onClicked: ThisDevice.installUpdate()
            }
        }

        // 下载的进度
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            visible: update.phase === "downloading"
            width: parent.width * ThisDevice.updateProgress
            height: 3
            color: Theme.text
        }
    }

    // ---- 连接到设备 ----
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: connectColumn.implicitHeight + 32
        radius: Theme.cardRadius
        color: Theme.card

        ColumnLayout {
            id: connectColumn
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: qsTr("连接到设备")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Theme.text
            }
            RowLayout {
                spacing: 8
                AppTextField {
                    id: address
                    Layout.fillWidth: true
                    placeholderText: qsTr("设备 ID、IP 或名称")
                    inputMethodHints: Qt.ImhNoPredictiveText
                    onTextChanged: error.visible = false
                    onAccepted: root.connectFromInput()
                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.surface
                    }
                }
                AppButton {
                    primary: true
                    text: qsTr("连接")
                    onClicked: root.connectFromInput()
                }
            }
            RowLayout {
                spacing: 10
                SegmentedControl {
                    Layout.preferredWidth: 170
                    color: Theme.fillHover
                    model: [qsTr("控制"), qsTr("仅查看")]
                    currentIndex: root.viewOnly ? 1 : 0
                    onCurrentIndexChanged: root.viewOnly = currentIndex === 1
                }
                Text {
                    id: error
                    Layout.fillWidth: true
                    visible: false
                    wrapMode: Text.Wrap
                    font.pixelSize: 13
                    color: Theme.danger
                }
            }
        }
    }

    // ---- 我的设备 ----
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        Text {
            Layout.fillWidth: true
            text: qsTr("我的设备")
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            visible: root.mine.length > 4
            text: qsTr("全部")
            font.pixelSize: 15
            color: Theme.textSecondary
            TapHandler { onTapped: root.allDevicesRequested() }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: mineColumn.implicitHeight
        radius: Theme.cardRadius
        color: Theme.card
        clip: true

        Column {
            id: mineColumn
            width: parent.width

            Repeater {
                model: root.mine.slice(0, 4)
                delegate: MobileDeviceRow {
                    id: row
                    required property var modelData
                    required property int index
                    width: mineColumn.width
                    device: row.modelData
                    first: row.index === 0
                    onClicked: root.connectRequested(row.modelData, root.viewOnly)
                    onPressAndHold: root.menuRequested(row.modelData)
                }
            }

            // 没有我的设备：没登录时说登录有什么用，登录了说别的电脑登录同一个账号以后会出现
            ColumnLayout {
                visible: root.mine.length === 0
                width: parent.width
                spacing: 10
                Item { Layout.preferredHeight: 8 }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    text: Account.signedIn ? qsTr("别的电脑登录同一个账号以后会出现在这里，不用密码就能连")
                                           : qsTr("登录以后，同一个账号的电脑会出现在这里，不用密码就能连")
                    wrapMode: Text.Wrap
                    font.pixelSize: 14
                    color: Theme.textSecondary
                }
                AppButton {
                    visible: !Account.signedIn
                    Layout.leftMargin: 16
                    dark: true
                    text: qsTr("登录 / 注册")
                    onClicked: root.loginRequested()
                }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }

    // ---- 最近 ----
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        visible: root.recent.length > 0
        Text {
            Layout.fillWidth: true
            text: qsTr("最近")
            font.pixelSize: 17
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            visible: root.recent.length > 3
            text: qsTr("全部")
            font.pixelSize: 15
            color: Theme.textSecondary
            TapHandler { onTapped: root.recentRequested() }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: root.recent.length > 0
        implicitHeight: recentColumn.implicitHeight
        radius: Theme.cardRadius
        color: Theme.card
        clip: true

        Column {
            id: recentColumn
            width: parent.width

            Repeater {
                model: root.recent.slice(0, 3)
                delegate: MobileDeviceRow {
                    id: recentRow
                    required property var modelData
                    required property int index
                    width: recentColumn.width
                    device: recentRow.modelData
                    first: recentRow.index === 0
                    detail: MobileFormat.lastSeen(recentRow.modelData.lastSeen)
                    onClicked: root.connectRequested(recentRow.modelData, root.viewOnly)
                    onPressAndHold: root.menuRequested(recentRow.modelData)
                }
            }
        }
    }
}
