import QtQuick
import QtQuick.Layouts
import "../main/device_query.js" as DeviceQuery

// 手机的设备页：电脑版侧栏里的我的设备、附近、收藏变成上面的分段；在线的一组、离线的一组。搜索名字、ID、IP
MobilePage {
    id: root

    title: qsTr("设备")

    // mine | nearby | favorites
    property string scope: "mine"

    signal connectRequested(var device, bool viewOnly)
    signal menuRequested(var device)

    readonly property var shown: DeviceQuery.query(Devices.devices, scope, { search: search.text, sort: "name" })
    readonly property var online: shown.filter(d => !d.presenceKnown || d.online)
    readonly property var offline: shown.filter(d => d.presenceKnown && !d.online)

    AppTextField {
        id: search
        Layout.fillWidth: true
        placeholderText: qsTr("搜索名称、ID 或 IP")
        inputMethodHints: Qt.ImhNoPredictiveText
        leftPadding: 40
        Icon {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "search"
            size: 18
            color: Theme.textMuted
        }
    }

    SegmentedControl {
        Layout.fillWidth: true
        model: [qsTr("我的设备"), qsTr("附近"), qsTr("收藏")]
        currentIndex: ["mine", "nearby", "favorites"].indexOf(root.scope)
        onCurrentIndexChanged: root.scope = ["mine", "nearby", "favorites"][currentIndex]
    }

    // 一组设备：小标题 + 卡片
    component Group: ColumnLayout {
        id: group
        property string heading
        property var devices: []
        Layout.fillWidth: true
        visible: devices.length > 0
        spacing: 6

        Text {
            Layout.leftMargin: 4
            text: group.heading
            font.pixelSize: Theme.captionFont
            color: Theme.textSecondary
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: rows.implicitHeight
            radius: Theme.cardRadius
            color: Theme.card
            clip: true

            Column {
                id: rows
                width: parent.width
                Repeater {
                    model: group.devices
                    delegate: MobileDeviceRow {
                        required property var modelData
                        required property int index
                        width: rows.width
                        device: modelData
                        first: index === 0
                        onClicked: root.connectRequested(modelData, false)
                        onPressAndHold: root.menuRequested(modelData)
                    }
                }
            }
        }
    }

    Group {
        Layout.topMargin: 4
        heading: root.offline.length > 0 ? qsTr("在线 · %1").arg(root.online.length) : qsTr("%1 台").arg(root.online.length)
        devices: root.online
    }
    Group {
        heading: qsTr("离线 · %1").arg(root.offline.length)
        devices: root.offline
    }

    // 一台都没有：说为什么
    Text {
        visible: root.shown.length === 0
        Layout.fillWidth: true
        Layout.topMargin: 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: search.text.trim().length > 0 ? qsTr("没有找到")
            : root.scope === "mine" ? (Account.signedIn ? qsTr("别的电脑登录同一个账号以后会出现在这里")
                                                        : qsTr("登录以后，同一个账号的电脑会出现在这里"))
            : root.scope === "nearby" ? (ThisDevice.lanDiscovery ? qsTr("同一个网络里开着比邻的电脑会出现在这里")
                                                                 : qsTr("设置里打开\"发现附近的设备\"以后，同一个网络里的电脑会出现在这里"))
            : qsTr("按住一台设备不放，选\"收藏\"")
        font.pixelSize: 15
        color: Theme.textSecondary
    }
}
