pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "../main/device_query.js" as DeviceQuery
import "mobile_format.js" as MobileFormat

// 手机的最近：连过的设备，最近连的在最上面，写着上次是什么时候
MobilePage {
    id: root

    title: qsTr("最近")


    signal connectRequested(var device, bool viewOnly)
    signal menuRequested(var device)

    readonly property var recent: DeviceQuery.query(Devices.devices, "recent", {})

    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 4
        visible: root.recent.length > 0
        implicitHeight: rows.implicitHeight
        radius: Theme.cardRadius
        color: Theme.card
        clip: true

        Column {
            id: rows
            width: parent.width
            Repeater {
                model: root.recent
                delegate: MobileDeviceRow {
                    id: row
                    required property var modelData
                    required property int index
                    width: rows.width
                    device: row.modelData
                    first: row.index === 0
                    detail: MobileFormat.lastSeen(row.modelData.lastSeen)
                    onClicked: root.connectRequested(row.modelData, false)
                    onPressAndHold: root.menuRequested(row.modelData)
                }
            }
        }
    }

    Text {
        visible: root.recent.length === 0
        Layout.fillWidth: true
        Layout.topMargin: 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: qsTr("连过的设备会出现在这里")
        font.pixelSize: 15
        color: Theme.textSecondary
    }
}
