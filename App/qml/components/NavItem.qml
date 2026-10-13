import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: root

    property string iconName
    property bool current: false

    implicitHeight: 34
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radius
        color: root.current ? Theme.selected : root.hovered ? Theme.hover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 10
        Item { Layout.preferredWidth: 2 }
        Icon {
            name: root.iconName
            size: 17
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            text: root.text
            font.pixelSize: 13
            color: Theme.text
        }
    }
}
