import QtQuick
import QtQuick.Layouts

// 手机的设置：最上面是账号卡片（登录着时点进账号页，没登录时去登录），下面本机、远程操作、通用。
// 电脑才有的（允许别人连接、开机时自动启动、收到的文件存到哪）手机上没有
MobilePage {
    id: root

    title: qsTr("设置")

    // 手指怎么操作电脑：touchpad | direct（MobileMain 按设置和手机、平板定）
    property string fingerMode: "touchpad"

    signal accountRequested()
    signal loginRequested()
    signal aboutRequested()

    // ---- 账号 ----
    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 4
        implicitHeight: 76
        radius: Theme.cardRadius
        color: accountTap.pressed ? Theme.cardHover : Theme.card

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 14
            spacing: 14

            Avatar {
                visible: Account.signedIn
                name: Account.displayName
                size: 46
            }
            Rectangle {
                visible: !Account.signedIn
                Layout.preferredWidth: 46
                Layout.preferredHeight: 46
                radius: 23
                color: Theme.avatar
                Icon {
                    anchors.centerIn: parent
                    name: "user"
                    size: 22
                    color: Theme.avatarText
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: Account.signedIn ? Account.displayName : qsTr("登录 / 注册")
                    elide: Text.ElideRight
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    // 本机也算一台
                    text: Account.signedIn ? qsTr("%1 · %2 台设备").arg(Account.email).arg(Account.devices.length + 1)
                                           : qsTr("登录以后，同一个账号的电脑不用密码就能连")
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }
            }
            Icon {
                name: "chevron-right"
                size: 16
                color: Theme.textMuted
            }
        }

        TapHandler {
            id: accountTap
            onTapped: Account.signedIn ? root.accountRequested() : root.loginRequested()
        }
    }

    // ---- 本机 ----
    SettingsSection {
        title: qsTr("本机")
        // 电脑上看到的这台手机的名字；清空就用回手机本来的名字
        SettingsRow {
            label: qsTr("本机名称")
            AppTextField {
                width: 170
                enabled: ThisDevice.serviceState === "ready"
                horizontalAlignment: TextInput.AlignRight
                fill: Theme.card
                focusFill: Theme.surface
                maximumLength: 64
                text: ThisDevice.deviceName
                onEditingFinished: {
                    ThisDevice.deviceName = text
                    text = Qt.binding(() => ThisDevice.deviceName)
                }
            }
        }
        SettingsRow {
            label: qsTr("发现附近的设备")
            detail: ThisDevice.lanDiscovery ? "" : qsTr("同一个网络里的电脑不会出现在\"附近\"")
            AppSwitch {
                enabled: ThisDevice.serviceState === "ready"
                checked: ThisDevice.lanDiscovery
                onToggled: ThisDevice.lanDiscovery = checked
            }
        }
    }

    // ---- 远程操作 ----
    RemoteControlSettings {
        fingerMode: root.fingerMode
    }

    // ---- 通用 ----
    SettingsSection {
        title: qsTr("通用")
        SettingsRow {
            label: qsTr("外观")
            SegmentedControl {
                width: 200
                model: [qsTr("跟随系统"), qsTr("浅色"), qsTr("深色")]
                currentIndex: ["system", "light", "dark"].indexOf(Appearance.scheme)
                onCurrentIndexChanged: Appearance.scheme = ["system", "light", "dark"][currentIndex]
            }
        }
        SettingsRow {
            label: qsTr("关于比邻")
            value: Qt.application.version
            navigates: true
            onClicked: root.aboutRequested()
        }
    }
}
