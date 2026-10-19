import QtQuick
import QtQuick.Layouts

// 设置 › 反馈问题 › 日志 › 一份日志的全文（Feedback.logText）
SettingsScroll {
    id: root

    // Feedback.logs 里的第几份
    property int index: -1

    title: index >= 0 && index < Feedback.logs.length ? Feedback.logs[index].name : ""
    backTo: qsTr("日志")

    onIndexChanged: scrollToTop()

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: logText.implicitHeight + 36
        radius: Theme.cardRadius
        color: Theme.card

        TextEdit {
            id: logText
            x: 18
            y: 18
            width: parent.width - 36
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.WrapAnywhere
            text: Feedback.logText(root.index)
            font.family: Qt.platform.os === "osx" || Qt.platform.os === "ios" ? "Menlo"
                       : Qt.platform.os === "windows" ? "Consolas" : "monospace"
            font.pixelSize: 12
            color: Theme.text
            selectionColor: Theme.textSelection
            selectedTextColor: Theme.text
        }
    }
}
