import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property int tab: 0
    // 这个面板属于哪个会话（SessionView）
    required property Item view
    readonly property ViewerSession session: view.session
    // 对方发来、还没决定收不收的文件有几份（文件页上标出来）
    property int fileOffers: 0

    color: Theme.surface

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 18

            UnderlineTab {
                iconName: "message"
                text: qsTr("对话")
                current: root.tab === 0
                onClicked: root.tab = 0

                // 有没看的消息
                Rectangle {
                    x: parent.width - 2
                    y: 9
                    width: 7
                    height: 7
                    radius: 3.5
                    color: Theme.danger
                    visible: root.session !== null && root.session.chat.conversation.unread > 0
                }
            }
            UnderlineTab {
                iconName: "folder"
                text: qsTr("文件")
                current: root.tab === 1
                onClicked: root.tab = 1

                // 对方发来了文件，等着决定
                Rectangle {
                    x: parent.width - 2
                    y: 9
                    width: 7
                    height: 7
                    radius: 3.5
                    color: Theme.danger
                    visible: root.fileOffers > 0
                }
            }
            UnderlineTab { iconName: "settings"; text: qsTr("设置"); current: root.tab === 2; onClicked: root.tab = 2 }
            Item { Layout.fillWidth: true }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.tab

            ChatTab {
                chat: root.session ? root.session.chat : null
                shown: root.tab === 0 && root.view.panelOpen && root.view.visible
            }
            FilesTab { session: root.view.session; name: root.view.name }
            SettingsTab { view: root.view }
        }
    }
}
