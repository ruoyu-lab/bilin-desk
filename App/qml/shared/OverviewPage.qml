import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import "device_query.js" as DeviceQuery

Item {
    id: root

    property bool viewOnly: false
    // 电脑版放进来的：缺授权的提示、本机的设备 ID 和临时密码（被别人连的那一面）
    property Component banner
    property Component thisDevice

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

            // 电脑版：缺授权时的提示（PermissionBanner）
            Loader {
                width: parent.width
                visible: item !== null && item.wanted
                sourceComponent: root.banner
            }

            RowLayout {
                width: parent.width
                spacing: 14

                // 电脑版：本机的设备 ID、临时密码（ThisDeviceCard）；平板上没有，"连接到设备"占满一行
                Loader {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 138
                    visible: item !== null
                    sourceComponent: root.thisDevice
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
                    hoverEnabled: Theme.hoverEffects
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
