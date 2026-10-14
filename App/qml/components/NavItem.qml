import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: root

    property string iconName
    property bool current: false

    implicitHeight: Theme.touch ? 44 : 34
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
            size: Theme.touch ? 20 : 17
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            text: root.text
            font.pixelSize: Theme.bodyFont
            color: Theme.text
        }
    }
}
