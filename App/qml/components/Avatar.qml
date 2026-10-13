import QtQuick

Rectangle {
    id: root

    property string name
    property int size: 32

    implicitWidth: size
    implicitHeight: size
    radius: size / 2
    color: Theme.avatar

    Text {
        anchors.centerIn: parent
        text: root.name.charAt(0).toUpperCase()
        font.pixelSize: root.size * 0.42
        font.weight: Font.Medium
        color: Theme.avatarText
    }
}
