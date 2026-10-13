import QtQuick
import QtQuick.Layouts

// 设置 › 关于比邻：版本和检查更新（装好的程序才有新版本），服务协议、隐私政策、开源声明；
// macOS 上最下面是卸载（Windows 在系统的"应用和功能"里卸载）
SettingsScroll {
    id: root

    title: qsTr("关于比邻")
    backTo: qsTr("设置")

    // terms | privacy | licenses
    signal documentRequested(string kind)
    signal uninstallRequested()

    readonly property string updateText: !ThisDevice.systemService ? ""
        : ThisDevice.updatePhase === "checking" ? qsTr("正在检查…")
        : ThisDevice.updateAvailable.length > 0 ? qsTr("有新版本 %1，在左侧导航栏里点一下就能更新").arg(ThisDevice.updateAvailable)
        : ThisDevice.updateChecked === "upToDate" ? qsTr("已经是最新版本")
        : ThisDevice.updateChecked === "failed" ? qsTr("连不上服务器，稍后再试") : ""

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 72
        radius: Theme.cardRadius
        color: Theme.card

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            Logo { size: 44 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("比邻")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("版本 %1").arg(Qt.application.version) + (root.updateText.length > 0 ? " · " + root.updateText : "")
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }
            AppButton {
                visible: ThisDevice.systemService
                enabled: ThisDevice.serviceState === "ready" && ThisDevice.updatePhase === "idle"
                text: qsTr("检查更新")
                onClicked: ThisDevice.checkUpdate()
            }
        }
    }

    SettingsSection {
        SettingsRow {
            label: qsTr("服务协议")
            navigates: true
            onClicked: root.documentRequested("terms")
        }
        SettingsRow {
            label: qsTr("隐私政策")
            navigates: true
            onClicked: root.documentRequested("privacy")
        }
        SettingsRow {
            label: qsTr("开源声明")
            navigates: true
            onClicked: root.documentRequested("licenses")
        }
    }

    // 只有装好的那一份有（开发版本没有）
    SettingsSection {
        visible: Qt.platform.os === "osx" && Setup.installed
        SettingsRow {
            label: qsTr("卸载比邻")
            danger: true
            navigates: true
            onClicked: root.uninstallRequested()
        }
    }
}
