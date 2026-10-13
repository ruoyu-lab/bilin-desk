import QtQuick

// 一组工具按钮。macOS：浅灰圆角底板里挨着排；Windows：没有底板，和窗口按钮一样平铺
Rectangle {
    default property alias content: row.data

    readonly property bool plated: Theme.unifiedTitleBar

    implicitWidth: row.implicitWidth + (plated ? 4 : 0)
    implicitHeight: 32
    radius: Theme.radius
    color: plated ? Theme.fill : "transparent"

    Row {
        id: row
        anchors.centerIn: parent
        spacing: parent.plated ? 2 : 4
    }
}
