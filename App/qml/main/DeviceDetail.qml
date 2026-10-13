import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var device

    signal connectRequested(bool viewOnly)

    readonly property var style: Theme.osStyle(device.os)
    // 网络唤醒（Wake）：叫不醒时为什么；我的设备才有
    readonly property string wakeProblem: {
        Wake.revision
        return device.mine && device.deviceId ? Wake.problem(device.deviceId) : ""
    }
    readonly property bool wakeable: device.mine && device.presenceKnown && !device.online && wakeProblem.length === 0
    // 发过唤醒包的那一台：等它上线（device 随设备列表刷新会换成新的对象，按 key 认）
    property string wokenKey
    readonly property bool wakeSent: wokenKey === device.key

    component InfoItem: ColumnLayout {
        property alias label: name.text
        property alias value: data.text
        Layout.fillWidth: true
        spacing: 3
        Text {
            id: name
            font.pixelSize: 12
            color: Theme.textMuted
        }
        Text {
            id: data
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: 13
            color: Theme.text
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(200, Math.round(width * 0.42))
            radius: 10
            color: root.style.tint

            Icon {
                anchors.centerIn: parent
                name: root.style.icon
                size: 44
                color: root.style.ink
                opacity: root.device.presenceKnown && !root.device.online ? 0.4 : 0.9
            }

            // 只有知道在不在线时（附近的）才显示
            Rectangle {
                x: 12
                y: 12
                visible: root.device.presenceKnown
                width: status.implicitWidth + 18
                height: 24
                radius: 12
                color: Qt.alpha(Theme.surface, 0.7)

                RowLayout {
                    id: status
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: root.device.online ? Theme.success : Theme.textMuted
                    }
                    Text {
                        text: root.device.online ? qsTr("在线") : qsTr("离线")
                        font.pixelSize: 12
                        color: Theme.text
                    }
                }
            }
        }

        RowLayout {
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: root.device.name
                    elide: Text.ElideRight
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    text: root.device.address
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }

            IconButton {
                iconName: root.device.favorite ? "star-filled" : "star"
                iconColor: root.device.favorite ? Theme.favorite : Theme.textSecondary
                text: root.device.favorite ? qsTr("取消收藏") : qsTr("收藏")
                onClicked: Devices.toggleFavorite(root.device.key)
            }
            // 不在线的我的设备：本机叫得醒时可以先叫醒它
            AppButton {
                visible: root.wakeable
                iconName: "power"
                text: root.wakeSent ? qsTr("已叫醒") : qsTr("唤醒")
                enabled: !root.wakeSent
                onClicked: {
                    if (Wake.wake(root.device.deviceId))
                        root.wokenKey = root.device.key
                }
            }
            AppButton {
                text: qsTr("仅查看")
                onClicked: root.connectRequested(true)
            }
            AppButton {
                primary: true
                text: qsTr("连接")
                onClicked: root.connectRequested(false)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 24
            rowSpacing: 14

            InfoItem { label: qsTr("设备 ID"); value: root.device.deviceId || qsTr("不知道（按 IP 地址添加的）") }
            InfoItem { label: qsTr("IP 地址"); value: root.device.ip || qsTr("不在附近") }
            InfoItem { label: qsTr("系统"); value: root.device.os || qsTr("未知") }
            InfoItem {
                label: qsTr("来源")
                value: root.device.saved && root.device.discovered ? qsTr("已保存 · 局域网")
                     : root.device.saved ? qsTr("已保存")
                     : root.device.discovered ? qsTr("局域网发现") : qsTr("手动连接")
            }
            InfoItem {
                label: qsTr("上次连接")
                value: root.device.lastSeen > 0 ? Theme.relativeTime(root.device.lastSeen) : qsTr("从未连接")
            }
            InfoItem {
                visible: root.device.mine
                Layout.columnSpan: 2
                label: qsTr("网络唤醒")
                value: root.wakeProblem.length === 0 ? qsTr("睡眠、关机以后能从本机叫醒") : root.wakeProblem
            }
            InfoItem {
                Layout.columnSpan: 2
                label: qsTr("配对")
                value: root.device.paired ? qsTr("已配对，连接不用密码") : qsTr("未配对")
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 8
            AppButton {
                iconName: "copy"
                text: root.device.deviceId ? qsTr("复制设备 ID") : qsTr("复制地址")
                onClicked: Clipboard.setText(root.device.target)
            }
            Item { Layout.fillWidth: true }
            AppButton {
                text: root.device.saved ? qsTr("移除设备") : qsTr("保存设备")
                onClicked: root.device.saved ? Devices.forget(root.device.key) : Devices.save(root.device.key)
            }
        }
    }
}
