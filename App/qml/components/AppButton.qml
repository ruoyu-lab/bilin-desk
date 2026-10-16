import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: root

    property string iconName
    property bool primary: false
    // 深色实心按钮：用在需要强调、又不想用强调色的地方（如登录）
    property bool dark: false
    // 危险操作：红色实心，如断开、删除
    property bool danger: false

    readonly property bool solid: primary || dark || danger

    implicitHeight: Theme.controlHeight
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: Theme.hoverEffects
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: Theme.radius
        color: root.danger ? (root.down ? Theme.dangerPressed : root.hovered ? Theme.dangerHover : Theme.danger)
             : root.dark ? (root.down ? Theme.inversePressed : root.hovered ? Theme.inverseHover : Theme.inverse)
             : root.primary ? (root.down ? Theme.accentPressed : root.hovered ? Theme.accentHover : Theme.accent)
             : (root.down ? Theme.selected : root.hovered ? Theme.fillHover : Theme.fill)
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 6

            Icon {
                visible: root.iconName.length > 0
                name: root.iconName
                size: 14
                color: root.dark ? Theme.inverseText : root.solid ? Theme.accentText : Theme.textSecondary
            }
            Text {
                text: root.text
                font.pixelSize: Theme.controlFont
                color: root.dark ? Theme.inverseText : root.solid ? Theme.accentText : Theme.text
            }
        }
    }
}
