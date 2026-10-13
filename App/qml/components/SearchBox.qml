import QtQuick

Rectangle {
    id: root

    property alias text: input.text
    property string placeholder: qsTr("搜索")
    // 按 Esc：清空并失去焦点；放在浮窗里时交给浮窗（关闭它）
    property bool clearOnEscape: true

    function focusInput() {
        input.forceActiveFocus()
        input.selectAll()
    }

    implicitWidth: 200
    implicitHeight: 32
    radius: Theme.radius
    color: input.activeFocus ? Theme.fillHover : Theme.fill

    Icon {
        id: glass
        x: 11
        anchors.verticalCenter: parent.verticalCenter
        name: "search"
        size: 14
        color: Theme.textMuted
    }

    TextInput {
        id: input
        anchors.left: glass.right
        anchors.leftMargin: 8
        anchors.right: clear.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        font.pixelSize: 13
        color: Theme.text
        selectByMouse: true
        selectionColor: Theme.textSelection
        selectedTextColor: Theme.text
        Keys.onEscapePressed: event => {
            if (!root.clearOnEscape) {
                event.accepted = false
                return
            }
            text = ""
            focus = false
        }

        Text {
            visible: input.text.length === 0
            text: root.placeholder
            font: input.font
            color: Theme.textMuted
        }
    }

    IconButton {
        id: clear
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 22
        implicitHeight: 22
        iconSize: 12
        compact: true
        visible: input.text.length > 0
        iconName: "x"
        onClicked: input.text = ""
    }
}
