import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 设置 › 反馈问题（docs/feedback.md）：说说遇到了什么、想要什么，没登录时可以留个联系方式，默认带上日志。
// 日志点进去（FeedbackLogs）能看全部内容，提交的就是看到的这些；版本和系统一起交上去
SettingsScroll {
    id: root

    title: qsTr("反馈问题")
    backTo: qsTr("设置")

    // 点了"查看日志"
    signal logsRequested()

    // 服务器那边的上限（Server/src/feedback.ts）
    readonly property int maxText: 5000
    readonly property int maxContact: 100

    // 刚提交上
    property bool sent: false
    property string error

    readonly property bool canSend: input.text.trim().length > 0 && input.text.length <= root.maxText
                                    && ThisDevice.serviceState === "ready" && !Feedback.busy
                                    && !(logSwitch.checked && Feedback.loadingLogs)

    // 从设置里点进来：重新读一遍日志（看日志再回来时不读，提交的是看到的那些）
    function start() {
        sent = false
        error = ""
        Feedback.loadLogs()
    }

    Connections {
        target: Feedback
        function onSent() {
            input.clear()
            contact.clear()
            root.sent = true
        }
        function onFailed(message) { root.error = message }
    }

    // ---- 填写 ----
    Text {
        Layout.fillWidth: true
        text: qsTr("遇到了问题，或者想要什么功能，都可以告诉我们。每一条都会认真看。")
        wrapMode: Text.Wrap
        font.pixelSize: 13
        color: Theme.textSecondary
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: Math.max(160, input.implicitHeight + 12)
        radius: Theme.cardRadius
        color: input.activeFocus ? Theme.surface : Theme.card
        border.width: input.activeFocus ? 1 : 0
        border.color: Theme.borderStrong

        // 铺满卡片（点空白处也能输入），底下留出字数那一行
        TextArea {
            id: input
            anchors.fill: parent
            anchors.bottomMargin: 12
            padding: 14
            wrapMode: TextEdit.Wrap
            placeholderText: qsTr("说说遇到了什么问题、怎么出现的；或者想要什么功能")
            placeholderTextColor: Theme.textMuted
            font.pixelSize: Theme.controlFont
            color: Theme.text
            selectionColor: Theme.textSelection
            selectedTextColor: Theme.text
            background: null
            onTextChanged: {
                root.sent = false
                root.error = ""
            }
        }
        Text {
            id: count
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            visible: input.length > root.maxText - 500
            text: qsTr("%1 / %2").arg(input.length).arg(root.maxText)
            font.pixelSize: Theme.captionFont
            color: input.length > root.maxText ? Theme.danger : Theme.textMuted
        }
    }

    SettingsSection {
        footer: qsTr("版本 %1 · %2 会一起提交").arg(Qt.application.version).arg(Feedback.systemName)

        // 登录着：服务器用账号的邮箱联系；没登录：自己留一个（藏起来的输入框也占位置，所以分成两行）
        SettingsRow {
            visible: Account.signedIn
            iconName: "mail"
            label: qsTr("联系方式")
            detail: qsTr("用账号的邮箱联系你")
            value: Account.email
        }
        SettingsRow {
            visible: !Account.signedIn
            iconName: "mail"
            label: qsTr("联系方式")
            detail: qsTr("选填")
            AppTextField {
                id: contact
                // 手机上一行放不下更宽的
                width: Theme.phone ? 180 : 220
                fill: Theme.surface
                focusFill: Theme.surface
                maximumLength: root.maxContact
                placeholderText: qsTr("邮箱、微信或 QQ")
            }
        }
        SettingsRow {
            iconName: "file"
            label: qsTr("附带日志")
            detail: qsTr("帮我们找出问题，不含画面、对话和文件内容")
            AppSwitch {
                id: logSwitch
                checked: true
            }
        }
        SettingsRow {
            visible: logSwitch.checked
            iconName: "eye"
            label: qsTr("查看日志")
            value: Feedback.loadingLogs ? qsTr("正在读取…")
                 : Feedback.logs.length === 0 ? qsTr("没有日志")
                 : qsTr("%1 份 · %2").arg(Feedback.logs.length).arg(Feedback.logsSize)
            navigates: Feedback.logs.length > 0
            onClicked: root.logsRequested()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: root.error.length > 0 ? root.error
                : root.sent ? qsTr("提交了，谢谢你的反馈！")
                : ThisDevice.serviceState !== "ready" ? qsTr("后台服务没有在运行") : ""
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: root.error.length > 0 || ThisDevice.serviceState !== "ready" ? Theme.danger : Theme.textSecondary
        }
        AppButton {
            dark: true
            enabled: root.canSend
            text: Feedback.busy ? qsTr("正在提交…") : qsTr("提交")
            onClicked: {
                root.error = ""
                Feedback.send(input.text, Account.signedIn ? "" : contact.text, logSwitch.checked)
            }
        }
    }
}
