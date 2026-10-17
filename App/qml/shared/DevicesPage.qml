import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import "device_query.js" as DeviceQuery

// 我的设备 / 附近 / 收藏 / 最近：左侧紧凑列表，右侧选中设备的详情
Item {
    id: root

    required property BrowseState browse

    signal connectRequested(var device, bool viewOnly)

    readonly property var groups: {
        const filter = { search: browse.search, onlyOnline: browse.onlyOnline, os: browse.os, sort: browse.sort }
        // 我的设备：同一个账号的，和自己保存的（按设备 ID、IP 地址添加的）；附近：同一个网络里发现的
        const scopes = browse.view === "favorites" ? [{ title: qsTr("收藏"), scope: "favorites" }]
                     : browse.view === "recent" ? [{ title: qsTr("最近连接"), scope: "recent" }]
                     : browse.view === "nearby" ? (ThisDevice.lanDiscovery ? [{ title: qsTr("附近"), scope: "discovered" }] : [])
                     : [{ title: qsTr("同一个账号"), scope: "mine" }, { title: qsTr("已保存"), scope: "saved" }]
        return scopes.map(s => ({ title: s.title, items: DeviceQuery.query(Devices.devices, s.scope, filter) }))
                     .filter(g => g.items.length > 0)
    }

    readonly property var selected: {
        let first = null
        for (const g of groups) {
            for (const d of g.items) {
                if (d.key === browse.selectedId)
                    return d
                first = first || d
            }
        }
        return first
    }

    // 右边详情的宽：拖到的（记着，AppSettings），不窄过最下面那排按钮挤不下的宽，左边列表至少留 ListMinimum
    readonly property int listMinimum: 220
    readonly property real detailMinimum: detailLoader.item ? (detailLoader.item as DeviceDetail).minimumWidth : 300
    function clampDetail(width) {
        return Math.max(detailMinimum, Math.min(width, row.width - handle.width - listMinimum))
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                id: flick
                anchors.fill: parent
                contentWidth: width
                contentHeight: list.height + 12
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: ScrollBar {}

                Column {
                    id: list
                    width: flick.width

                    Repeater {
                        model: root.groups
                        delegate: Column {
                            id: group
                            required property var modelData
                            required property int index
                            width: list.width
                            spacing: 3

                            Text {
                                leftPadding: 8
                                topPadding: group.index === 0 ? 4 : 16
                                bottomPadding: 3
                                text: group.modelData.title
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: Theme.textMuted
                            }

                            Repeater {
                                model: group.modelData.items
                                delegate: DeviceRow {
                                    required property var modelData
                                    width: group.width
                                    device: modelData
                                    selected: root.selected !== null && root.selected.key === modelData.key
                                    onSelectRequested: root.browse.selectedId = modelData.key
                                    onConnectRequested: viewOnly => root.connectRequested(modelData, viewOnly)
                                }
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -40
                visible: root.groups.length === 0
                text: root.browse.search.length > 0 || root.browse.filtering ? qsTr("没有符合条件的设备")
                    : root.browse.view === "favorites" ? qsTr("还没有收藏的设备")
                    : root.browse.view === "recent" ? qsTr("还没有连接记录")
                    : root.browse.view === "nearby" ? (ThisDevice.lanDiscovery ? qsTr("同一个网络里还没有发现别的电脑")
                                                                               : qsTr("设置里关掉了发现附近的设备"))
                    : qsTr("还没有设备")
                font.pixelSize: 13
                color: Theme.textMuted
            }
        }

        // 列表和详情中间的空隙：按住左右拖改详情的宽，只有指针变成左右箭头
        MouseArea {
            id: handle
            property real pressX
            property real pressWidth

            Layout.preferredWidth: 12
            Layout.fillHeight: true
            visible: root.browse.detailOpen
            cursorShape: Qt.SizeHorCursor
            preventStealing: true
            onPressed: mouse => {
                pressX = mapToItem(null, mouse.x, 0).x
                pressWidth = detail.width
            }
            onPositionChanged: mouse => {
                const delta = pressX - mapToItem(null, mouse.x, 0).x
                AppSettings.deviceDetailWidth = Math.round(root.clampDetail(pressWidth + delta))
            }
        }

        Rectangle {
            id: detail
            Layout.preferredWidth: root.clampDetail(AppSettings.deviceDetailWidth)
            Layout.fillHeight: true
            visible: root.browse.detailOpen
            radius: Theme.cardRadius
            color: Theme.card

            Loader {
                id: detailLoader
                anchors.fill: parent
                active: root.selected !== null
                sourceComponent: DeviceDetail {
                    device: root.selected
                    onConnectRequested: viewOnly => root.connectRequested(root.selected, viewOnly)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.selected === null
                text: qsTr("选择一个设备查看详情")
                font.pixelSize: 13
                color: Theme.textMuted
            }
        }
    }
}
