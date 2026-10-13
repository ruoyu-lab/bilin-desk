import QtQuick
import QtQuick.Controls

Switch {
    id: root

    implicitWidth: Theme.switchWidth
    implicitHeight: Theme.switchHeight
    padding: 0

    indicator: Rectangle {
        width: Theme.switchWidth
        height: Theme.switchHeight
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.switchOff

        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            x: root.checked ? parent.width - width - 2 : 2
            y: 2
            width: parent.height - 4
            height: width
            radius: width / 2
            color: "white"
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
}
