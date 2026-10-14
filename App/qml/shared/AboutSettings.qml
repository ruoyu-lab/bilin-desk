import QtQuick
import QtQuick.Layouts

// 设置 › 关于比邻：版本和检查更新（装好的程序才有新版本），服务协议、隐私政策、开源声明；
// macOS 上最下面是卸载（Windows 在系统的"应用和功能"里卸载）
SettingsScroll {
    id: root

    title: qsTr("关于比邻")
    backTo: qsTr("设置")

    // 最下面有"卸载比邻"（macOS 装好的程序：系统没有卸载入口，程序自己给）
    property bool uninstallable: false

    // terms | privacy | licenses
    signal documentRequested(string kind)
    signal uninstallRequested()

    readonly property string updateText: !ThisDevice.updatable ? ""
        : ThisDevice.updatePhase === "checking" ? qsTr("正在检查…")
        : ThisDevice.updatePhase === "downloading" ? qsTr("正在下载 %1%").arg(Math.floor(ThisDevice.updateProgress * 100))
        : ThisDevice.updatePhase === "installing" ? qsTr("正在安装")
        : ThisDevice.updateFailed === "download" ? qsTr("上次没下载成，再试一次")
        : ThisDevice.updateFailed === "install" ? qsTr("上次没装上，再试一次")
        : ThisDevice.updateAvailable.length > 0 ? qsTr("有新版本 %1").arg(ThisDevice.updateAvailable)
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
            // 有新版本时就地更新（电脑上左边的导航栏里也有）
            AppButton {
                readonly property bool available: ThisDevice.updateAvailable.length > 0
                visible: ThisDevice.updatable
                enabled: ThisDevice.serviceState === "ready" && ThisDevice.updatePhase === "idle"
                dark: available
                text: available ? qsTr("更新") : qsTr("检查更新")
                onClicked: available ? ThisDevice.installUpdate() : ThisDevice.checkUpdate()
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

    SettingsSection {
        visible: root.uninstallable
        SettingsRow {
            label: qsTr("卸载比邻")
            danger: true
            navigates: true
            onClicked: root.uninstallRequested()
        }
    }
}
