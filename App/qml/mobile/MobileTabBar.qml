pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// 手机（窄的时候）底部的标签栏：概览、设备、最近、设置。宽的时候换成左边的侧栏（MobileNavRail）
Rectangle {
    id: root

    property int currentIndex
    readonly property var tabs: [
        { icon: "home", title: qsTr("概览") },
        { icon: "monitor", title: qsTr("设备") },
        { icon: "clock", title: qsTr("最近") },
        { icon: "settings", title: qsTr("设置") }
    ]

    signal selected(int index)

    implicitHeight: 58 + SafeArea.margins.bottom
    color: Theme.bg

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 58
        spacing: 0

        Repeater {
            model: root.tabs
            delegate: Item {
                id: tab
                required property var modelData
                required property int index
                readonly property bool current: tab.index === root.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 3
                    Icon {
                        Layout.alignment: Qt.AlignHCenter
                        name: tab.modelData.icon
                        size: 24
                        color: tab.current ? Theme.text : Theme.textMuted
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: tab.modelData.title
                        font.pixelSize: 11
                        font.weight: tab.current ? Font.Medium : Font.Normal
                        color: tab.current ? Theme.text : Theme.textMuted
                    }
                }

                TapHandler {
                    onTapped: root.selected(tab.index)
                }
            }
        }
    }
}
