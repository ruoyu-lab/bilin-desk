import QtQuick
import QtQuick.Effects

Item {
    id: root

    property string name
    property color color: Theme.textSecondary
    property int size: 18

    implicitWidth: size
    implicitHeight: size

    Image {
        id: glyph
        anchors.fill: parent
        source: root.name.length > 0 ? Qt.resolvedUrl("../icons/" + root.name + ".svg") : ""
        sourceSize: Qt.size(root.size * 3, root.size * 3)
        visible: false
    }

    MultiEffect {
        anchors.fill: glyph
        source: glyph
        colorization: 1.0
        colorizationColor: root.color
    }
}
