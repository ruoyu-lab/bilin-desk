import QtQuick

Rectangle {
    id: root

    property string os
    property int size: 32
    readonly property var style: Theme.osStyle(os)

    implicitWidth: size
    implicitHeight: size
    radius: Math.round(size * 0.3)
    color: style.tint

    Icon {
        anchors.centerIn: parent
        name: root.style.icon
        size: Math.round(root.size * 0.5)
        color: root.style.ink
    }
}
