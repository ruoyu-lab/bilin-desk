import QtQuick
import QtQuick.Layouts

// 设置 › 账号（登录着时才进得来）：昵称、邮箱、改密码；账号下别的电脑（移除：那台电脑退出这个账号）；退出、注销
SettingsScroll {
    id: root

    title: qsTr("账号")
    backTo: qsTr("设置")

    // 点了移除、在等结果；没移除成功（比如连不上服务器）说原因
    property bool removing: false
    property string removeError

    onVisibleChanged: if (!visible) removeError = ""

    Connections {
        target: Account
        function onChanged() { root.removing = false }
        function onFailed(message) {
            if (!root.removing)
                return
            root.removing = false
            root.removeError = message
        }
    }

    SettingsSection {
        SettingsRow {
            iconName: "user"
            label: qsTr("昵称")
            AppTextField {
                width: 200
                fill: Theme.surface
                focusFill: Theme.surface
                maximumLength: 32
                text: Account.displayName
                onEditingFinished: {
                    const nickname = text.trim()
                    if (nickname.length > 0 && nickname !== Account.displayName)
                        Account.setNickname(nickname)
                    text = Qt.binding(() => Account.displayName)
                }
            }
        }
        SettingsRow {
            iconName: "mail"
            label: qsTr("邮箱")
            value: Account.email
        }
        SettingsRow {
            iconName: "key"
            label: qsTr("密码")
            AppButton {
                text: qsTr("修改")
                onClicked: accountDialog.show("password")
            }
        }
    }

    SettingsSection {
        title: qsTr("我的设备")
        footer: Account.devices.length > 0 ? qsTr("移除以后，那台电脑会退出这个账号") : ""
        SettingsRow {
            visible: Account.devices.length === 0
            iconName: "monitor"
            label: qsTr("还没有别的电脑")
            detail: qsTr("别的电脑登录同一个账号以后会出现在这里")
        }
        Repeater {
            model: Account.devices
            delegate: SettingsRow {
                required property var modelData
                iconName: modelData.platform === "macOS" ? "apple" : modelData.platform === "Windows" ? "windows" : "monitor"
                label: modelData.name
                detail: (modelData.online ? qsTr("在线 · ") : "") + modelData.platform + " · " + modelData.deviceId
                AppButton {
                    enabled: !root.removing
                    text: qsTr("移除")
                    onClicked: {
                        root.removeError = ""
                        root.removing = true
                        Account.removeDevice(modelData.deviceId)
                    }
                }
            }
        }
        SettingsRow {
            visible: root.removeError.length > 0
            label: qsTr("没移除")
            detail: root.removeError
            danger: true
        }
    }

    SettingsSection {
        SettingsRow {
            iconName: "log-out"
            label: qsTr("退出登录")
            navigates: true
            onClicked: accountDialog.show("signOut")
        }
        SettingsRow {
            iconName: "trash"
            label: qsTr("注销账号")
            danger: true
            navigates: true
            onClicked: accountDialog.show("delete")
        }
    }

    AccountDialog {
        id: accountDialog
    }
}
