import QtQuick

// 远程窗口侧边面板的"对话"：和这个会话的对方（和这台设备的对话记录，下次连上还在）
ChatView {
    id: root

    required property ViewerChat chat
    // 面板正显示着这一页：对方的消息算已读
    property bool shown: false
    readonly property Conversation conversation: chat ? chat.conversation : null

    messages: conversation ? conversation.messages : []
    notice: conversation ? conversation.storeError : ""
    send: text => root.chat.send(text)

    onShownChanged: if (shown && conversation) conversation.markRead()
    Connections {
        target: root.conversation
        function onUnreadChanged() {
            if (root.shown && root.conversation.unread > 0)
                root.conversation.markRead()
        }
    }
}
