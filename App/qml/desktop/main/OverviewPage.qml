import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import "../../shared/device_query.js" as DeviceQuery

Item {
    id: root

    property bool viewOnly: false
    property string copied

    signal connectRequested(var device, bool viewOnly)
    signal detailsRequested(string listView, string key)
    signal addRequested

    readonly property var mine: DeviceQuery.query(Devices.devices, "mine", { sort: "online" })
    readonly property var nearby: DeviceQuery.query(Devices.devices, "nearby", { sort: "online" })
    readonly property var favorites: DeviceQuery.query(Devices.devices, "favorites", { sort: "name" })

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
        root.connectRequested(device, root.viewOnly)
        address.clear()
    }

    function copy(key, value) {
        Clipboard.setText(value)
        copied = key
        resetCopied.restart()
    }

    Timer {
        id: resetCopied
        interval: 1500
        onTriggered: root.copied = ""
    }

    component SectionTitle: Row {
        property alias title: heading.text
        property alias note: detail.text
        spacing: 8
        Text {
            id: heading
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            id: detail
            anchors.baseline: heading.baseline
            font.pixelSize: 12
            color: Theme.textMuted
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + 32
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: 24
            y: 4
            width: flick.width - 48
            spacing: 14

            // macOS：被控需要"屏幕与系统音频录制"（画面和声音）和"辅助功能"两个权限
            Rectangle {
                id: permissionBanner

                readonly property bool screenMissing: !IncomingConnections.screenPermission
                readonly property bool inputMissing: !IncomingConnections.inputPermission

                width: parent.width
                height: 48
                visible: ThisDevice.serviceState === "ready" && ThisDevice.acceptIncoming && (screenMissing || inputMissing)
                radius: Theme.cardRadius
                color: Theme.card

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 10
                    spacing: 10

                    Icon {
                        name: "key"
                        size: 16
                        color: Theme.warning
                    }
                    Text {
                        Layout.fillWidth: true
                        text: permissionBanner.screenMissing && permissionBanner.inputMissing
                              ? qsTr("对方要看到、听到并控制本机，需要授予\"屏幕与系统音频录制\"和\"辅助功能\"权限")
                              : permissionBanner.screenMissing ? qsTr("对方要看到、听到本机，需要授予\"屏幕与系统音频录制\"权限")
                              : qsTr("对方要控制本机，需要授予\"辅助功能\"权限")
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: Theme.text
                    }
                    AppButton {
                        text: qsTr("去授权")
                        onClicked: IncomingConnections.requestPermission(permissionBanner.screenMissing ? "screen" : "input")
                    }
                }
            }

            RowLayout {
                width: parent.width
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 138
                    radius: Theme.cardRadius
                    color: Theme.card

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 4

                        Text {
                            text: qsTr("本机设备 ID")
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }
                        // 窗口窄了 ID 和复制按钮放不下时，按钮不占位置：鼠标移到这一行上才盖在 ID 右端出现
                        Item {
                            id: idRow
                            readonly property bool tight: idText.implicitWidth + 4 + copyId.implicitWidth > width
                            Layout.fillWidth: true
                            implicitHeight: Math.max(idText.implicitHeight, copyId.implicitHeight)

                            HoverHandler { id: idHover }
                            Text {
                                id: idText
                                anchors.verticalCenter: parent.verticalCenter
                                text: ThisDevice.deviceId.length > 0 ? ThisDevice.deviceId : "—"
                                font.pixelSize: 26
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1
                                color: Theme.text
                            }
                            // 盖在 ID 上时垫一层卡片的底色，数字不从图标后面透出来
                            Rectangle {
                                anchors.fill: copyId
                                visible: copyId.visible && idRow.tight
                                color: Theme.card
                            }
                            IconButton {
                                id: copyId
                                // ID 比这一行还宽（伸进卡片边距）时跟着 ID 的右端，整个盖住最后几个字
                                x: Math.max(idRow.width, idText.width) - width
                                anchors.verticalCenter: parent.verticalCenter
                                visible: ThisDevice.deviceId.length > 0 && (!idRow.tight || idHover.hovered)
                                iconName: root.copied === "id" ? "check" : "copy"
                                text: qsTr("复制设备 ID")
                                onClicked: root.copy("id", ThisDevice.deviceId.replace(/\s+/g, ""))
                            }
                        }
                        Item { Layout.fillHeight: true }
                        // 别人现在连不上本机时（后台服务还没连上、起不来，关了别人连接，缺授权，出错了），
                        // 临时密码那一行换成一句说明
                        RowLayout {
                            id: statusRow
                            readonly property string serviceState: ThisDevice.serviceState
                            readonly property bool failed: serviceState === "unavailable"
                                                           || IncomingConnections.problem === "unavailable"
                            visible: serviceState !== "ready" || !ThisDevice.acceptIncoming || !IncomingConnections.accepting
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: statusRow.serviceState === "connecting" ? qsTr("正在准备…")
                                    : statusRow.serviceState === "unavailable" ? qsTr("后台服务没能启动，本机现在不能被连接")
                                    : !ThisDevice.acceptIncoming ? qsTr("已关闭别人连接，可在设置中开启")
                                    : IncomingConnections.problem === "permission" ? qsTr("还没授权，别人现在连不上本机")
                                    : IncomingConnections.problem === "unavailable" ? qsTr("出错了，别人现在连不上本机")
                                    : qsTr("正在准备…")
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: statusRow.failed ? Theme.danger : Theme.textMuted
                            }
                            Text {
                                visible: statusRow.failed
                                text: qsTr("重试")
                                font.pixelSize: 12
                                color: Theme.accent
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: statusRow.serviceState === "unavailable" ? ThisDevice.retry()
                                                                                        : IncomingConnections.retry()
                                }
                            }
                        }
                        RowLayout {
                            visible: ThisDevice.serviceState === "ready" && ThisDevice.acceptIncoming && IncomingConnections.accepting
                            spacing: 4
                            Text {
                                text: qsTr("临时密码")
                                font.pixelSize: 12
                                color: Theme.textSecondary
                            }
                            Text {
                                Layout.leftMargin: 4
                                Layout.fillWidth: true
                                text: ThisDevice.password
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                font.letterSpacing: 1
                                color: Theme.text
                            }
                            IconButton {
                                iconName: "refresh"
                                text: qsTr("刷新密码")
                                onClicked: ThisDevice.regeneratePassword()
                            }
                            IconButton {
                                iconName: root.copied === "password" ? "check" : "copy"
                                text: qsTr("复制临时密码")
                                onClicked: root.copy("password", ThisDevice.password)
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 13
                    Layout.preferredHeight: 138
                    radius: Theme.cardRadius
                    color: Theme.card

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10

                        Text {
                            text: qsTr("连接到设备")
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }
                        RowLayout {
                            spacing: 8
                            AppTextField {
                                id: address
                                Layout.fillWidth: true
                                placeholderText: qsTr("设备 ID、IP 或名称")
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
                                Layout.preferredWidth: 150
                                implicitHeight: 28
                                color: Theme.fillHover
                                model: [qsTr("控制"), qsTr("仅查看")]
                                currentIndex: root.viewOnly ? 1 : 0
                                onCurrentIndexChanged: root.viewOnly = currentIndex === 1
                            }
                            Text {
                                id: error
                                Layout.fillWidth: true
                                visible: false
                                text: qsTr("请输入设备 ID 或地址")
                                font.pixelSize: 12
                                color: Theme.danger
                            }
                        }
                    }
                }
            }

            // 登录了账号、账号下有别的设备时
            SectionTitle {
                visible: root.mine.length > 0
                topPadding: 14
                title: qsTr("我的设备")
                note: qsTr("同一个账号登录的设备")
            }

            AdaptiveGrid {
                id: mineGrid
                visible: root.mine.length > 0
                width: parent.width
                height: implicitHeight
                count: mineRepeater.count
                minCellWidth: 210
                cellHeight: 60

                Repeater {
                    id: mineRepeater
                    model: root.mine
                    delegate: DeviceTile {
                        required property var modelData
                        required property int index
                        device: modelData
                        subtitle: (modelData.online ? qsTr("在线") : qsTr("离线")) + " · " + modelData.deviceId
                        x: mineGrid.cellX(index)
                        y: mineGrid.cellY(index)
                        width: mineGrid.cellWidth
                        height: mineGrid.cellHeight
                        onDetailsRequested: root.detailsRequested("mine", modelData.key)
                        onConnectRequested: viewOnly => root.connectRequested(modelData, viewOnly)
                    }
                }
            }

            SectionTitle {
                visible: ThisDevice.lanDiscovery
                topPadding: 14
                title: qsTr("附近")
                note: root.nearby.length > 0 ? qsTr("发现 %1 台").arg(root.nearby.length)
                    : root.mine.some(d => d.discovered) ? qsTr("没有别的设备")
                    : qsTr("暂未发现局域网设备")
            }

            AdaptiveGrid {
                id: nearbyGrid
                visible: ThisDevice.lanDiscovery
                width: parent.width
                height: implicitHeight
                count: nearbyRepeater.count
                minCellWidth: 210
                cellHeight: 60

                Repeater {
                    id: nearbyRepeater
                    model: root.nearby
                    delegate: DeviceTile {
                        required property var modelData
                        required property int index
                        device: modelData
                        subtitle: qsTr("在线") + " · " + modelData.address
                        x: nearbyGrid.cellX(index)
                        y: nearbyGrid.cellY(index)
                        width: nearbyGrid.cellWidth
                        height: nearbyGrid.cellHeight
                        onDetailsRequested: root.detailsRequested("nearby", modelData.key)
                        onConnectRequested: viewOnly => root.connectRequested(modelData, viewOnly)
                    }
                }
            }

            SectionTitle {
                topPadding: 14
                title: qsTr("收藏")
                note: root.favorites.length > 0 ? "" : qsTr("在设备详情里点星标即可收藏")
            }

            AdaptiveGrid {
                id: favoriteGrid
                width: parent.width
                height: implicitHeight
                count: favoriteRepeater.count + 1
                minCellWidth: 150
                cellHeight: 100

                Repeater {
                    id: favoriteRepeater
                    model: root.favorites
                    delegate: DeviceTile {
                        required property var modelData
                        required property int index
                        device: modelData
                        stacked: true
                        subtitle: modelData.online ? qsTr("在线")
                                : modelData.lastSeen > 0 ? Theme.relativeTime(modelData.lastSeen) : modelData.address
                        x: favoriteGrid.cellX(index)
                        y: favoriteGrid.cellY(index)
                        width: favoriteGrid.cellWidth
                        height: favoriteGrid.cellHeight
                        onDetailsRequested: root.detailsRequested("favorites", modelData.key)
                        onConnectRequested: viewOnly => root.connectRequested(modelData, viewOnly)
                    }
                }

                AbstractButton {
                    id: addTile
                    x: favoriteGrid.cellX(favoriteRepeater.count)
                    y: favoriteGrid.cellY(favoriteRepeater.count)
                    width: favoriteGrid.cellWidth
                    height: favoriteGrid.cellHeight
                    hoverEnabled: true
                    onClicked: root.addRequested()

                    background: Rectangle {
                        radius: 10
                        color: addTile.hovered ? Theme.card : Theme.inset
                    }
                    contentItem: Item {
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "plus"
                                size: 15
                                color: Theme.textMuted
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("添加设备")
                                font.pixelSize: 13
                                color: Theme.textSecondary
                            }
                        }
                    }
                }
            }
        }
    }
}
