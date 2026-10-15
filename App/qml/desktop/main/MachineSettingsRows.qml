import QtQuick
import QtQuick.Layouts

// 电脑版设置页"本机"里的两行（SettingsPage 的 machineRows）：被别人连的开关、开机时自动启动
ColumnLayout {
    spacing: 0

    SettingsRow {
        iconName: "link"
        label: qsTr("允许别人连接本机")
        AppSwitch {
            enabled: ThisDevice.serviceState === "ready"
            checked: ThisDevice.acceptIncoming
            onToggled: ThisDevice.acceptIncoming = checked
        }
    }
    // 关着时说后果；系统设置里没允许时说明，给个按钮去那里打开
    SettingsRow {
        iconName: "power"
        label: qsTr("开机时自动启动")
        detail: Autostart.problem === "approval" ? qsTr("系统设置里没允许，开机时不会启动")
              : Autostart.problem === "refused" ? qsTr("没改成，可能被系统或安全软件拦下了")
              : Autostart.enabled ? "" : qsTr("只有打开程序时才能被连接")
        RowLayout {
            spacing: 10
            AppButton {
                visible: Autostart.problem === "approval"
                text: qsTr("打开系统设置")
                onClicked: Autostart.openSystemSettings()
            }
            AppSwitch {
                enabled: Autostart.problem !== "approval"
                checked: Autostart.enabled
                onToggled: Autostart.enabled = checked
            }
        }
    }
}
