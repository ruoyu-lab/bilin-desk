import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: root

    property string iconName
    property bool current: false

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 4
    hoverEnabled: true

    background: Item {
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 2
            radius: 1
            color: root.current ? Theme.text : "transparent"
        }
    }

    contentItem: RowLayout {
        id: row
        spacing: 5
        Icon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 14
            color: root.current || root.hovered ? Theme.text : Theme.textSecondary
        }
        Text {
            text: root.text
            font.pixelSize: 13
            font.weight: root.current ? Font.Medium : Font.Normal
            color: root.current || root.hovered ? Theme.text : Theme.textSecondary
        }
    }
}
