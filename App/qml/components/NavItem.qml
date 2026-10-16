import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: root

    property string iconName
    property bool current: false

    implicitHeight: Theme.phone ? 44 : 34
    hoverEnabled: Theme.hoverEffects

    background: Rectangle {
        radius: Theme.radius
        color: root.current ? Theme.selected : root.hovered || root.down ? Theme.hover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 10
        Item { Layout.preferredWidth: 2 }
        Icon {
            name: root.iconName
            size: Theme.phone ? 20 : 17
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
