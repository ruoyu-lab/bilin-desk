import QtQuick
import QtQuick.Layouts

// 对话：消息列表和输入框。对方的消息在左边（浅底），自己的在右边（强调色），没发出去的下面写原因。
// messages：[{ incoming, text, error }]（Conversation 的 messages）
ColumnLayout {
    id: root

    property var messages: []
    property string placeholder: qsTr("和对方说点什么")
    // 输入框上面的一行说明（对话记录没存下来时）
    property string notice
    // 发出去：返回空字符串表示成功，否则是失败的原因
    property var send: function (text) { return "" }

    property string error

    function submit() {
        const text = input.text.trim()
        if (text.length === 0)
            return
        error = root.send(text)
        if (error.length === 0)
            input.clear()
    }

    spacing: 0
    onMessagesChanged: Qt.callLater(() => list.positionViewAtEnd())

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 14
        spacing: 8
        clip: true
        model: root.messages

        delegate: Column {
            id: bubble
            required property var modelData
            width: ListView.view.width
            spacing: 3

            Rectangle {
                anchors.right: bubble.modelData.incoming ? undefined : parent.right
                width: Math.min(label.implicitWidth + 24, parent.width * 0.85)
                height: label.implicitHeight + 14
                radius: 14
                color: bubble.modelData.incoming ? Theme.fill : Theme.accent

                Text {
                    id: label
                    anchors.fill: parent
                    anchors.topMargin: 7
                    anchors.bottomMargin: 7
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    text: bubble.modelData.text
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.bodyFont
                    color: bubble.modelData.incoming ? Theme.text : Theme.accentText
                }
            }
            Text {
                anchors.right: parent.right
                width: Math.min(implicitWidth, parent.width * 0.85)
                visible: bubble.modelData.error.length > 0
                text: qsTr("没有发出去：%1").arg(bubble.modelData.error)
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: Theme.danger
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: root.placeholder
            font.pixelSize: Theme.bodyFont
            color: Theme.textMuted
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        visible: root.error.length > 0
        text: qsTr("没有发出去：%1").arg(root.error)
        wrapMode: Text.Wrap
        font.pixelSize: Theme.captionFont
        color: Theme.danger
    }

    Text {
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        visible: root.notice.length > 0
        text: root.notice
        wrapMode: Text.Wrap
        font.pixelSize: Theme.captionFont
        color: Theme.textSecondary
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 12
        spacing: 6

        AppTextField {
            id: input
            Layout.fillWidth: true
            placeholderText: qsTr("输入消息")
            maximumLength: 4000
            onAccepted: root.submit()
            onTextChanged: root.error = ""
        }
        IconButton {
            implicitWidth: 34
            implicitHeight: 34
            iconName: "send"
            text: qsTr("发送")
            filled: true
            onClicked: root.submit()
        }
    }
}
