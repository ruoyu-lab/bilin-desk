pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 设置 › 反馈问题（docs/feedback.md）：说说遇到了什么、想要什么，没登录时可以留个联系方式，默认带上日志。
// 日志点进去能看全部内容，提交的就是看到的这些；版本和系统一起交上去
Item {
    id: root

    // 同设置页（SettingsScroll）
    property bool overTitleBar: false
    property real leftInset: 0

    signal backRequested

    // 服务器那边的上限（Server/src/feedback.ts）
    readonly property int maxText: 5000
    readonly property int maxContact: 100

    // 正在看日志：列表（-1）或者其中一份（下标）
    property bool viewingLogs: false
    property int logIndex: -1
    // 刚提交上
    property bool sent: false
    property string error

    readonly property int logBytes: Feedback.logs.reduce((total, log) => total + log.bytes, 0)
    readonly property bool canSend: input.text.trim().length > 0 && input.text.length <= root.maxText
                                    && ThisDevice.serviceState === "ready" && !Feedback.busy
                                    && !(logSwitch.checked && Feedback.loadingLogs)

    function sizeText(bytes) {
        return bytes < 1024 ? qsTr("%1 B").arg(bytes)
             : bytes < 1024 * 1024 ? qsTr("%1 KB").arg(Math.ceil(bytes / 1024))
             : qsTr("%1 MB").arg((bytes / 1024 / 1024).toFixed(1))
    }

    function back() {
        if (logIndex >= 0)
            logIndex = -1
        else if (viewingLogs)
            viewingLogs = false
        else
            root.backRequested()
        scroll.scrollToTop()
    }

    onVisibleChanged: {
        if (visible) {
            Feedback.loadLogs()
        } else {
            viewingLogs = false
            logIndex = -1
            sent = false
            error = ""
        }
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

    SettingsScroll {
        id: scroll
        anchors.fill: parent
        title: root.logIndex >= 0 ? Feedback.logs[root.logIndex].name : root.viewingLogs ? qsTr("日志") : qsTr("反馈问题")
        backTo: root.logIndex >= 0 ? qsTr("日志") : root.viewingLogs ? qsTr("反馈问题") : qsTr("设置")
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.back()

        // ---- 填写 ----
        Text {
            Layout.fillWidth: true
            visible: !root.viewingLogs
            text: qsTr("遇到了问题，或者想要什么功能，都可以告诉我们。每一条都会认真看。")
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
        }

        Rectangle {
            Layout.fillWidth: true
            visible: !root.viewingLogs
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
            visible: !root.viewingLogs
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
                AppTextField {
                    id: contact
                    width: 220
                    fill: Theme.surface
                    focusFill: Theme.surface
                    maximumLength: root.maxContact
                    placeholderText: qsTr("邮箱、微信或 QQ（选填）")
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
                     : qsTr("%1 份 · %2").arg(Feedback.logs.length).arg(root.sizeText(root.logBytes))
                navigates: Feedback.logs.length > 0
                onClicked: {
                    root.viewingLogs = true
                    scroll.scrollToTop()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !root.viewingLogs
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

        // ---- 日志的列表 ----
        Text {
            Layout.fillWidth: true
            visible: root.viewingLogs && root.logIndex < 0
            text: qsTr("提交时带上的就是这些：比邻这次和上次运行时记下的连接、出错的情况，每份最多最后 512 KB。")
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        SettingsSection {
            visible: root.viewingLogs && root.logIndex < 0
            Repeater {
                model: Feedback.logs
                delegate: SettingsRow {
                    required property var modelData
                    required property int index
                    label: modelData.name
                    value: root.sizeText(modelData.bytes)
                    navigates: true
                    onClicked: {
                        root.logIndex = index
                        scroll.scrollToTop()
                    }
                }
            }
        }

        // ---- 一份日志 ----
        Rectangle {
            Layout.fillWidth: true
            visible: root.logIndex >= 0
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
                text: root.logIndex >= 0 ? Feedback.logText(root.logIndex) : ""
                font.family: Qt.platform.os === "osx" || Qt.platform.os === "ios" ? "Menlo"
                           : Qt.platform.os === "windows" ? "Consolas" : "monospace"
                font.pixelSize: 12
                color: Theme.text
                selectionColor: Theme.textSelection
                selectedTextColor: Theme.text
            }
        }
    }
}
