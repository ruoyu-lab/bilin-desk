import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 设置 › 账号里的"修改密码""注销账号"：输密码，等服务器回答。成了就关掉，没成（密码不对、连不上服务器）
// 在下面说为什么。"退出登录"不用密码，只确认一下
Popup {
    id: root

    // password：修改密码 | delete：注销账号 | signOut：退出登录
    property string mode
    property string error

    function show(kind) {
        mode = kind
        error = ""
        current.clear()
        fresh.clear()
        confirm.clear()
        open()
        if (mode !== "signOut")
            current.forceActiveFocus()
    }

    function submit() {
        if (Account.busy)
            return
        if (mode === "signOut") {
            Account.signOut()
            return
        }
        if (current.text.length === 0)
            return fail(qsTr("请输入密码"), current)
        if (mode === "delete") {
            Account.deleteAccount(current.text)
            return
        }
        if (fresh.text.length < 8)
            return fail(qsTr("新密码至少 8 位"), fresh)
        if (confirm.text !== fresh.text)
            return fail(qsTr("两次输入的新密码不一致"), confirm)
        Account.changePassword(current.text, fresh.text)
    }

    function fail(message, field) {
        error = message
        field.forceActiveFocus()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 340
    padding: 20
    modal: true
    closePolicy: Account.busy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: Theme.scrim }
    background: PopupSurface { radius: 14 }

    Connections {
        target: Account
        enabled: root.opened
        function onFailed(message) { root.error = message }
        function onPasswordChanged() { root.close() }
        function onChanged() {
            if ((root.mode === "delete" || root.mode === "signOut") && !Account.signedIn)
                root.close()
        }
    }

    component Field: AppTextField {
        Layout.fillWidth: true
        implicitHeight: 36
        echoMode: TextInput.Password
        maximumLength: 128
        onTextEdited: root.error = ""
        onAccepted: root.submit()
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: root.mode === "delete" ? qsTr("注销账号") : root.mode === "signOut" ? qsTr("退出登录") : qsTr("修改密码")
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            visible: root.mode === "delete"
            text: qsTr("账号 %1 和它在服务器上的信息会被删掉，登录着这个账号的电脑都不再属于它，"
                       + "\"我的设备\"也就没有了。注销以后不能恢复。").arg(Account.email)
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        Text {
            Layout.fillWidth: true
            visible: root.mode === "signOut"
            text: qsTr("退出以后，本机不再属于账号 %1：你的其他电脑再连本机，要用临时密码。").arg(Account.email)
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        Field {
            id: current
            visible: root.mode !== "signOut"
            placeholderText: root.mode === "delete" ? qsTr("输入密码确认") : qsTr("现在的密码")
        }
        Field {
            id: fresh
            visible: root.mode === "password"
            placeholderText: qsTr("新密码（至少 8 位）")
        }
        Field {
            id: confirm
            visible: root.mode === "password"
            placeholderText: qsTr("再输一遍新密码")
        }
        Text {
            Layout.fillWidth: true
            visible: root.error.length > 0
            text: root.error
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Theme.danger
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton {
                enabled: !Account.busy
                text: qsTr("取消")
                onClicked: root.close()
            }
            AppButton {
                enabled: !Account.busy
                danger: root.mode === "delete"
                primary: root.mode === "password"
                dark: root.mode === "signOut"
                text: Account.busy ? qsTr("请稍候…") : root.mode === "delete" ? qsTr("注销")
                    : root.mode === "signOut" ? qsTr("退出") : qsTr("修改")
                onClicked: root.submit()
            }
        }
    }
}
