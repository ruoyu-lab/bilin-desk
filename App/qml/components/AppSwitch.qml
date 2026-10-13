import QtQuick
import QtQuick.Controls

Switch {
    id: root

    implicitWidth: 34
    implicitHeight: 20
    padding: 0

    indicator: Rectangle {
        width: 34
        height: 20
        radius: 10
        color: root.checked ? Theme.accent : Theme.switchOff

        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            x: root.checked ? 16 : 2
            y: 2
            width: 16
            height: 16
            radius: 8
            color: "white"
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
}
