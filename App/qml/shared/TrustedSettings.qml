import QtQuick
import QtQuick.Layouts

// 设置 › 信任的设备（电脑版）：别人连本机时点允许那会儿勾了"以后不要密码"的设备（ThisDevice.trustedDevices）。
// 每台能改来连时要不要点允许，也能取消信任（之后它连本机又要临时密码）
SettingsScroll {
    id: root

    title: qsTr("信任的设备")
    backTo: qsTr("设置")

    Text {
        Layout.fillWidth: true
        text: qsTr("别人连本机时，在允许连接的卡片上打开\"以后这台设备连本机不要密码\"，那台设备就会出现在这里。"
                   + "它只靠设备自己的密钥证明身份，别的设备冒充不了；关掉\"允许别人连接本机\"时它也连不进来。")
        wrapMode: Text.Wrap
        font.pixelSize: 13
        color: Theme.textSecondary
    }

    SettingsSection {
        SettingsRow {
            visible: ThisDevice.trustedDevices.length === 0
            iconName: "key"
            label: qsTr("还没有信任的设备")
        }
        Repeater {
            model: ThisDevice.trustedDevices
            delegate: SettingsRow {
                id: row
                required property var modelData
                iconName: modelData.platform === "macOS" ? "apple" : modelData.platform === "Windows" ? "windows" : "monitor"
                label: modelData.name
                detail: qsTr("%1 · %2 信任").arg(modelData.deviceId).arg(Qt.formatDate(modelData.since, "yyyy/M/d"))
                RowLayout {
                    spacing: 10
                    Text {
                        text: qsTr("不用点允许")
                        font.pixelSize: 13
                        color: Theme.textSecondary
                    }
                    AppSwitch {
                        enabled: ThisDevice.serviceState === "ready"
                        checked: !row.modelData.ask
                        onToggled: ThisDevice.setTrustAsk(row.modelData.rawId, !checked)
                    }
                    AppButton {
                        enabled: ThisDevice.serviceState === "ready"
                        text: qsTr("取消信任")
                        onClicked: ThisDevice.untrust(row.modelData.rawId)
                    }
                }
            }
        }
    }
}
