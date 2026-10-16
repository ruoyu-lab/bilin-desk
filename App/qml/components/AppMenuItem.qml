import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MenuItem {
    id: root

    property bool markable: false
    property bool marked: false
    property bool destructive: false
    // 右边浅色的一小段：快捷键这样的
    property string hint

    implicitHeight: Theme.phone ? 44 : 30
    leftPadding: 12
    rightPadding: 12

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            radius: 6
            color: root.highlighted ? Theme.hover : "transparent"
        }
    }

    indicator: Icon {
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: root.marked
        name: "check"
        size: 13
        color: Theme.text
    }

    contentItem: RowLayout {
        spacing: 24

        Text {
            Layout.fillWidth: true
            leftPadding: root.markable ? 20 : 0
            text: root.text
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Theme.bodyFont
            color: root.destructive ? Theme.danger : root.enabled ? Theme.text : Theme.textMuted
        }
        Text {
            visible: root.hint.length > 0
            text: root.hint
            font.pixelSize: 12
            color: Theme.textMuted
        }
    }
}
