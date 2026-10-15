import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: root

    property string accountName
    // 点了退出登录、在等结果：退出了就关掉；没退出成功（比如连不上服务器）留着显示原因
    property bool signingOut: false
    property string error

    width: 260
    padding: 16

    onAboutToShow: {
        signingOut = false
        error = ""
    }

    Connections {
        target: Account
        function onChanged() {
            if (root.signingOut && !Account.signedIn)
                root.close()
        }
        function onFailed(message) {
            if (!root.signingOut)
                return
            root.signingOut = false
            root.error = message
        }
    }

    background: PopupSurface {}

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            spacing: 10
            Avatar { name: root.accountName; size: 36 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: root.accountName
                    elide: Text.ElideRight
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    visible: Account.signedIn
                    text: Account.email
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
                Text {
                    text: qsTr("本机 %1").arg(ThisDevice.deviceId)
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }
        }

        AppButton {
            Layout.fillWidth: true
            visible: !Account.signedIn
            primary: true
            text: qsTr("登录 / 注册")
            onClicked: {
                root.close()
                Account.loginRequested = true
            }
        }

        AppButton {
            Layout.fillWidth: true
            visible: Account.signedIn
            enabled: !root.signingOut
            text: qsTr("退出登录")
            onClicked: {
                root.error = ""
                root.signingOut = true
                Account.signOut()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.error.length > 0
            text: root.error
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Theme.danger
        }
    }
}
