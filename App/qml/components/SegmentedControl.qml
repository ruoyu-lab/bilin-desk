import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0

    readonly property int inset: 2
    readonly property real segmentWidth: root.model.length > 0
                                         ? (width - inset * 2 - inset * (model.length - 1)) / model.length : 0

    implicitHeight: Theme.segmentHeight
    radius: Theme.radius
    color: Theme.fill

    // 选中块：切换时滑动到新位置
    Rectangle {
        x: root.inset + root.currentIndex * (root.segmentWidth + root.inset)
        y: root.inset
        width: root.segmentWidth
        height: root.height - root.inset * 2
        radius: Theme.radius - 2
        color: Theme.surface

        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.inset
        spacing: root.inset

        Repeater {
            model: root.model
            delegate: AbstractButton {
                id: segment
                required property int index
                required property string modelData
                readonly property bool current: index === root.currentIndex

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                onClicked: root.currentIndex = index

                background: null
                contentItem: Text {
                    text: segment.modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: Theme.phone ? 14 : 12
                    color: segment.current ? Theme.text : Theme.textSecondary

                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }
        }
    }
}
