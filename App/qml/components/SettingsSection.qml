import QtQuick
import QtQuick.Layouts

// 设置页里的一组：上面一个小标题（可以没有），一张浅灰底的卡片，卡片里是一行一行的设置（SettingsRow），
// 下面一句说明（可以没有）
ColumnLayout {
    id: root

    default property alias rows: card.data
    property alias title: heading.text
    property alias footer: note.text

    Layout.fillWidth: true
    spacing: 6

    Text {
        id: heading
        Layout.leftMargin: 6
        visible: text.length > 0
        font.pixelSize: Theme.captionFont
        color: Theme.textSecondary
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight + 8
        radius: Theme.cardRadius
        color: Theme.card

        ColumnLayout {
            id: card
            anchors.fill: parent
            anchors.margins: 4
            spacing: 0
        }
    }
    Text {
        id: note
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        visible: text.length > 0
        wrapMode: Text.Wrap
        font.pixelSize: Theme.captionFont
        color: Theme.textSecondary
    }
}
