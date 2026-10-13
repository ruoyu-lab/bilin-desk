import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 让自己的另一台电脑重启、关机、睡眠、注销之前的确认气泡，弹在右上角的电源按钮下方：只有一句话和两个按钮
Popup {
    id: root

    property string name
    // "restart" | "shutDown" | "sleep" | "signOut"
    property string action

    signal confirmed(string action)

    function openAt(anchor, action) {
        root.action = action
        parent = anchor
        x = anchor.width - width
        y = anchor.height + 8
        open()
    }

    readonly property var copy: ({
        restart: { title: qsTr("重启「%1」？"), button: qsTr("重启") },
        shutDown: { title: qsTr("关闭「%1」？"), button: qsTr("关机") },
        sleep: { title: qsTr("让「%1」睡眠？"), button: qsTr("睡眠") },
        signOut: { title: qsTr("注销「%1」上的用户？"), button: qsTr("注销") },
    })
    readonly property var current: copy[action] ?? copy.restart

    width: 280
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: PopupSurface {}

    contentItem: ColumnLayout {
        spacing: 14

        Text {
            Layout.fillWidth: true
            text: root.current.title.arg(root.name)
            wrapMode: Text.Wrap
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: Theme.text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                Layout.fillWidth: true
                text: qsTr("取消")
                onClicked: root.close()
            }
            AppButton {
                Layout.fillWidth: true
                danger: true
                text: root.current.button
                onClicked: {
                    root.close()
                    root.confirmed(root.action)
                }
            }
        }
    }
}
