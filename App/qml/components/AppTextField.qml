import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: root

    // 底色：默认浅灰、聚焦时深一点；放在灰色背景上时改成白色
    property color fill: Theme.fill
    property color focusFill: Theme.fillHover

    implicitHeight: Theme.fieldHeight
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: Theme.controlFont
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.textSelection
    selectedTextColor: Theme.text

    background: Rectangle {
        radius: Theme.radius
        color: root.activeFocus ? root.focusFill : root.fill
    }
}
