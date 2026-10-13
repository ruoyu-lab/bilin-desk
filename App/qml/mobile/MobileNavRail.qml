pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// 平板横着、宽的时候左边的侧栏：和电脑版一样的导航条目（NavItem），四项同底部标签栏（MobileTabBar）
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

    implicitWidth: 220
    color: Theme.sidebar

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.SafeArea.margins.top + 16
        anchors.bottomMargin: root.SafeArea.margins.bottom + 12
        anchors.leftMargin: root.SafeArea.margins.left + 12
        anchors.rightMargin: 12
        spacing: 4

        Repeater {
            model: root.tabs
            delegate: NavItem {
                id: item
                required property var modelData
                required property int index
                Layout.fillWidth: true
                iconName: item.modelData.icon
                text: item.modelData.title
                current: item.index === root.currentIndex
                onClicked: root.selected(item.index)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
