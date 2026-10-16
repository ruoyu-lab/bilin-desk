import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 登录 / 注册 / 找回密码的表单（电脑的登录窗口 LoginWindow 右边、手机的登录页 MobileLogin 用）。
// 注册、找回密码分两步：先填表，服务器往邮箱发验证码，再输验证码（docs/devices-and-accounts.md）
Item {
    id: form

    // login | register | reset（找回密码）
    property string page: "login"
    readonly property bool registering: page === "register"
    readonly property bool resetting: page === "reset"
    // 验证码已经发出去了，在等人输入（注册、找回密码的第二步）
    property bool codeStep: false
    // 验证码发到了哪个邮箱
    property string codeEmail
    // 还要等几秒才能再发一个验证码（服务器一分钟只给同一个邮箱发一个）
    property int resendWait: 0
    property string error
    property bool agreed: false

    // 表单上面的大标题（窗口标题也用）
    readonly property string title: registering ? qsTr("注册") : resetting ? qsTr("找回密码") : qsTr("登录")

    // 点了"以后再说"：这次不登录
    signal skipRequested()

    onPageChanged: reset()
    Component.onCompleted: reset()

    // 换了一页：从第一步开始（邮箱留着）
    function reset() {
        error = ""
        codeStep = false
        nickname.clear()
        password.clear()
        confirm.clear()
        code.clear()
        // 手机上不一打开就弹出键盘
        if (!Theme.touch)
            (registering ? nickname : identifier).forceActiveFocus()
    }

    // 这一步要不要先同意协议：登录、注册的第一步、找回密码设新密码那一步（重设以后就登录了）
    readonly property bool needsAgreement: page === "login" || (registering && !codeStep) || (resetting && codeStep)

    function submit() {
        if (Account.busy)
            return
        if (codeStep && !/^\d{6}$/.test(code.text))
            return fail(qsTr("请输入邮件里的 6 位验证码"), code)
        if (registering && !codeStep && nickname.text.trim().length === 0)
            return fail(qsTr("请输入昵称"), nickname)
        const id = identifier.text.trim()
        if (!codeStep) {
            if (id.length === 0)
                return fail(qsTr("请输入邮箱"), identifier)
            if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(id))
                return fail(qsTr("邮箱格式不正确"), identifier)
        }
        if (password.visible) {
            if (password.text.length === 0)
                return fail(qsTr("请输入密码"), password)
            if (confirm.visible && password.text.length < 8)
                return fail(qsTr("密码至少 8 位"), password)
            if (confirm.visible && confirm.text !== password.text)
                return fail(qsTr("两次输入的密码不一致"), confirm)
        }
        if (needsAgreement && !agreed) {
            error = qsTr("请先阅读并同意服务协议和隐私政策")
            return
        }

        error = ""
        if (page === "login")
            Account.signIn(id, password.text)
        else if (!codeStep)
            sendCode(id)
        else if (registering)
            Account.registerAccount(nickname.text.trim(), codeEmail, password.text, code.text)
        else
            Account.resetPassword(codeEmail, code.text, password.text)
    }

    function sendCode(email) {
        codeEmail = email
        Account.sendCode(email, registering ? "register" : "resetPassword")
    }

    function fail(message, field) {
        error = message
        field.forceActiveFocus()
    }

    Connections {
        target: Account
        function onFailed(message) { form.error = message }
        function onCodeSent() {
            form.error = ""
            form.resendWait = 60
            if (!form.codeStep) {
                form.codeStep = true
                code.clear()
            }
            code.forceActiveFocus()
        }
    }

    Timer {
        running: form.resendWait > 0
        interval: 1000
        repeat: true
        onTriggered: --form.resendWait
    }

    component Input: AppTextField {
        Layout.fillWidth: true
        implicitHeight: Theme.phone ? Theme.fieldHeight : 40
        onTextEdited: form.error = ""
    }

    component PasswordInput: Input {
        id: input
        property bool revealed: false
        rightPadding: 40
        echoMode: revealed ? TextInput.Normal : TextInput.Password
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 28
            implicitHeight: 28
            iconName: "eye"
            active: input.revealed
            text: input.revealed ? qsTr("隐藏密码") : qsTr("显示密码")
            onClicked: input.revealed = !input.revealed
        }
    }

    component Link: Text {
        signal activated
        font.pixelSize: 13
        color: Theme.accent
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.activated()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Text {
            text: form.registering ? (form.codeStep ? qsTr("验证邮箱") : qsTr("创建账号"))
                : form.resetting ? (form.codeStep ? qsTr("设新密码") : qsTr("找回密码")) : qsTr("登录")
            font.pixelSize: 24
            font.weight: Font.DemiBold
            color: Theme.text
        }
        // 第一步：换到别的页；输验证码时：发到了哪个邮箱，可以回去改
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 5
            spacing: 4
            // 长邮箱放不下时中间省略，"返回修改"紧跟在后面
            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth
                text: form.codeStep ? qsTr("验证码已发到 %1").arg(form.codeEmail)
                    : form.registering ? qsTr("已有账号？") : form.resetting ? qsTr("想起来了？") : qsTr("还没有账号？")
                elide: Text.ElideMiddle
                font.pixelSize: 13
                color: Theme.textSecondary
            }
            Link {
                text: form.codeStep ? qsTr("返回修改") : form.page === "login" ? qsTr("注册") : qsTr("登录")
                onActivated: {
                    if (form.codeStep) {
                        form.codeStep = false
                        form.error = ""
                    } else {
                        form.page = form.page === "login" ? "register" : "login"
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        Input {
            id: nickname
            visible: form.registering && !form.codeStep
            placeholderText: qsTr("昵称")
            maximumLength: 32
            onAccepted: identifier.forceActiveFocus()
        }
        Input {
            id: identifier
            visible: !form.codeStep
            placeholderText: qsTr("邮箱")
            maximumLength: 254
            onAccepted: password.visible ? password.forceActiveFocus() : form.submit()
        }
        Input {
            id: code
            visible: form.codeStep
            placeholderText: qsTr("邮件里的 6 位验证码")
            maximumLength: 6
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /\d{0,6}/ }
            onAccepted: password.visible ? password.forceActiveFocus() : form.submit()
        }
        PasswordInput {
            id: password
            visible: form.page === "login" || form.codeStep === form.resetting
            placeholderText: form.page === "login" ? qsTr("密码")
                : form.resetting ? qsTr("新密码（至少 8 位）") : qsTr("密码（至少 8 位）")
            maximumLength: 128
            onAccepted: confirm.visible ? confirm.forceActiveFocus() : form.submit()
        }
        PasswordInput {
            id: confirm
            visible: form.page !== "login" && form.codeStep === form.resetting
            placeholderText: form.resetting ? qsTr("确认新密码") : qsTr("确认密码")
            maximumLength: 128
            onAccepted: form.submit()
        }

        // 出错的说明；右边：登录时"忘记密码？"，输验证码时重新发送。都没有时这一行不占位置，按钮紧跟输入框
        RowLayout {
            Layout.fillWidth: true
            visible: form.error.length > 0 || form.page === "login" || form.codeStep
            Text {
                Layout.fillWidth: true
                text: form.error
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: Theme.danger
            }
            Text {
                visible: form.page === "login" && form.error.length === 0
                text: qsTr("忘记密码？")
                font.pixelSize: 12
                color: Theme.textSecondary
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: form.page = "reset"
                }
            }
            Text {
                visible: form.codeStep
                text: form.resendWait > 0 ? qsTr("%1 秒后可以重新发送").arg(form.resendWait) : qsTr("重新发送")
                font.pixelSize: 12
                color: form.resendWait > 0 || Account.busy ? Theme.textMuted : Theme.accent
                MouseArea {
                    anchors.fill: parent
                    enabled: form.resendWait === 0 && !Account.busy
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: form.sendCode(form.codeEmail)
                }
            }
        }

        AppButton {
            Layout.fillWidth: true
            implicitHeight: Theme.phone ? Theme.controlHeight : 40
            dark: true
            enabled: !Account.busy
            text: Account.busy ? qsTr("请稍候…")
                : form.page === "login" ? qsTr("登录")
                : !form.codeStep ? (form.registering ? qsTr("注册") : qsTr("发送验证码"))
                : form.registering ? qsTr("完成注册") : qsTr("重设密码并登录")
            onClicked: form.submit()
        }

        AbstractButton {
            id: agreement
            visible: form.needsAgreement
            Layout.fillWidth: true
            implicitHeight: 20
            checkable: true
            checked: form.agreed
            onToggled: {
                form.agreed = checked
                form.error = ""
            }

            contentItem: RowLayout {
                spacing: 8
                Rectangle {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    radius: 5
                    color: agreement.checked ? Theme.inverse : Theme.fillHover
                    Icon {
                        anchors.centerIn: parent
                        visible: agreement.checked
                        name: "check"
                        size: 11
                        color: Theme.inverseText
                    }
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.StyledText
                    text: qsTr("我已阅读并同意 <a href=\"terms\">服务协议</a> 和 <a href=\"privacy\">隐私政策</a>")
                    linkColor: Theme.accent
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    onLinkActivated: link => documentPopup.show(link)
                    HoverHandler {
                        enabled: parent.hoveredLink.length > 0
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            Text {
                text: qsTr("不登录也能用 ·")
                font.pixelSize: 13
                color: Theme.textSecondary
            }
            Text {
                text: qsTr("以后再说 →")
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.text
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: form.skipRequested()
                }
            }
        }
    }

    // 服务协议、隐私政策的全文（App/legal）
    Popup {
        id: documentPopup

        property string kind

        function show(link) {
            kind = link
            open()
            body.contentY = 0
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 48)
        height: Math.min(560, parent.height - 48)
        padding: 20
        modal: true
        Overlay.modal: Rectangle { color: Theme.scrim }
        background: PopupSurface { radius: 14 }

        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: documentPopup.kind === "privacy" ? qsTr("隐私政策") : qsTr("服务协议")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Flickable {
                id: body
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: legal.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: ScrollBar {}

                LegalText {
                    id: legal
                    width: body.width - 12
                    kind: documentPopup.kind
                }
            }
            AppButton {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 6
                text: qsTr("好的")
                onClicked: documentPopup.close()
            }
        }
    }
}
