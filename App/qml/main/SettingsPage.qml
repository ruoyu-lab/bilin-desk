import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore

// 设置：最上面是账号卡片（登录着时点进账号页，没登录时去登录），下面"本机"四项、"通用"三项。
// 不常用的收在点进去的下一页里：账号（昵称、密码、我的设备、退出、注销）、关于比邻（版本、协议、开源声明、卸载）
Item {
    id: root

    // macOS：页面盖在窗口的标题栏底下，顶上只留 30px 拖动窗口
    property bool overTitleBar: false
    // 页面左上角被红绿灯、侧栏开关压住的宽度，标题要让开
    property real leftInset: 0

    // 点了"卸载比邻"（macOS：没有系统的卸载入口，程序自己给）
    signal uninstallRequested()

    // 点进去的下一页：account（账号）| about（关于比邻）| terms、privacy（服务协议、隐私政策）| licenses（开源声明）；
    // 空是设置本身。离开设置时回到设置本身
    property string subpage
    onVisibleChanged: if (!visible) subpage = ""

    // 在账号页时退出、注销了：回到设置
    Connections {
        target: Account
        function onChanged() {
            if (!Account.signedIn && root.subpage === "account")
                root.subpage = ""
        }
    }

    readonly property url downloads: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
    // 收到的文件存到的文件夹，显示它的名字
    readonly property string folderName: {
        const url = AppSettings.downloadFolder.toString()
        return url.length > 0 ? decodeURIComponent(url.replace(/\/$/, "").split("/").pop()) : qsTr("下载")
    }

    SettingsScroll {
        anchors.fill: parent
        visible: root.subpage.length === 0
        title: qsTr("设置")
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset

        // ---- 账号 ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 68
            radius: Theme.cardRadius
            color: Account.signedIn && accountHover.hovered ? Theme.cardHover : Theme.card

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                Avatar {
                    visible: Account.signedIn
                    name: Account.displayName
                    size: 40
                }
                Rectangle {
                    visible: !Account.signedIn
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 20
                    color: Theme.avatar
                    Icon {
                        anchors.centerIn: parent
                        name: "user"
                        size: 18
                        color: Theme.avatarText
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: Account.signedIn ? Account.displayName : qsTr("没有登录")
                        elide: Text.ElideRight
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        // 本机也算一台
                        text: Account.signedIn ? qsTr("%1 · %2 台设备").arg(Account.email).arg(Account.devices.length + 1)
                                               : qsTr("登录以后，同一个账号的电脑不用密码就能互相连")
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }
                Text {
                    visible: Account.signedIn
                    text: qsTr("账号")
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }
                Icon {
                    visible: Account.signedIn
                    name: "chevron-right"
                    size: 14
                    color: Theme.textMuted
                }
                AppButton {
                    visible: !Account.signedIn
                    dark: true
                    text: qsTr("登录 / 注册")
                    onClicked: Account.loginRequested = true
                }
            }

            HoverHandler {
                id: accountHover
                enabled: Account.signedIn
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                enabled: Account.signedIn
                onTapped: root.subpage = "account"
            }
        }

        // ---- 本机 ----
        SettingsSection {
            title: qsTr("本机")
            // 别人连本机时看到的名字；清空就用回电脑本来的名字
            SettingsRow {
                iconName: "monitor"
                label: qsTr("本机名称")
                AppTextField {
                    width: 200
                    enabled: ThisDevice.serviceState === "ready"
                    fill: Theme.surface
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
            SettingsRow {
                iconName: "radar"
                label: qsTr("发现附近的设备")
                detail: ThisDevice.lanDiscovery ? "" : qsTr("附近的设备也看不到本机")
                AppSwitch {
                    enabled: ThisDevice.serviceState === "ready"
                    checked: ThisDevice.lanDiscovery
                    onToggled: ThisDevice.lanDiscovery = checked
                }
            }
        }

        // ---- 通用 ----
        SettingsSection {
            title: qsTr("通用")
            SettingsRow {
                iconName: "palette"
                label: qsTr("外观")
                SegmentedControl {
                    width: 210
                    model: [qsTr("跟随系统"), qsTr("浅色"), qsTr("深色")]
                    currentIndex: ["system", "light", "dark"].indexOf(Appearance.scheme)
                    onCurrentIndexChanged: Appearance.scheme = ["system", "light", "dark"][currentIndex]
                }
            }
            SettingsRow {
                iconName: "folder"
                label: qsTr("收到的文件存到")
                value: root.folderName
                navigates: true
                onClicked: folderPicker.open()
            }
            SettingsRow {
                iconName: "info"
                label: qsTr("关于比邻")
                value: Qt.application.version
                navigates: true
                onClicked: root.subpage = "about"
            }
        }
    }

    AccountSettings {
        anchors.fill: parent
        visible: root.subpage === "account"
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.subpage = ""
    }

    AboutSettings {
        anchors.fill: parent
        visible: root.subpage === "about"
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.subpage = ""
        onDocumentRequested: kind => root.subpage = kind
        onUninstallRequested: root.uninstallRequested()
    }

    LicensesPage {
        anchors.fill: parent
        visible: root.subpage === "licenses"
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.subpage = "about"
    }

    LegalPage {
        anchors.fill: parent
        visible: root.subpage === "terms" || root.subpage === "privacy"
        kind: visible ? root.subpage : ""
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.subpage = "about"
    }

    // 选了系统的"下载"就记成默认，不记具体的路径
    FolderDialog {
        id: folderPicker
        title: qsTr("收到的文件存到")
        currentFolder: AppSettings.downloadFolder.toString().length > 0 ? AppSettings.downloadFolder : root.downloads
        onAccepted: AppSettings.downloadFolder = selectedFolder.toString() === root.downloads.toString() ? "" : selectedFolder
    }
}
