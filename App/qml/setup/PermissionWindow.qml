import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// macOS 第一次打开时的授权引导（在主窗口之前）："屏幕与系统音频录制"和"辅助功能"只有用户自己能在系统设置里打开。
// 一页两行，点"去打开"跳到系统设置里的那一页；后台服务定时看授权，回到这里时打开了的那一行打勾。
// 两个都打开后点"完成"进入程序，也可以"以后再说"；之后不再出现（AppSettings.permissionGuideDone）。
// 系统要求重新打开程序时（打开录制权限后常见），重新打开后还是这一页，打开了的那一行已经打勾
ApplicationWindow {
    id: win

    readonly property bool ready: ThisDevice.serviceState === "ready"
    readonly property bool granted: ready && IncomingConnections.screenPermission && IncomingConnections.inputPermission
    // 这次点过"去打开"：没点过就都有了（比如早就授权过）的，直接进入程序
    property bool asked: false

    // 引导完了（完成、以后再说）
    signal finished

    function finish() {
        AppSettings.permissionGuideDone = true
        finished()
    }

    onGrantedChanged: if (granted && !asked) finish()
    // 打开时就都有了：等创建它的人接上 finished 再说
    Component.onCompleted: {
        WindowChrome.applyTitleBar(win, "compact")
        if (granted)
            Qt.callLater(finish)
    }

    // 固定大小（同登录窗口：限制用常量，不绑到 width / height）
    width: 560
    height: 400
    minimumWidth: 560
    maximumWidth: 560
    minimumHeight: 400
    maximumHeight: 400
    title: qsTr("授权")
    color: Theme.bg
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    // 一行授权：一句大白话，没打开时一个按钮，打开了一个勾
    component PermissionRow: Rectangle {
        id: row

        property string icon
        property string name
        property string purpose
        property bool allowed
        // 后台服务在了才能去打开（授权提示由它弹）
        property bool available
        signal openRequested

        Layout.fillWidth: true
        implicitHeight: 64
        radius: Theme.cardRadius
        color: Theme.card

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 14
            spacing: 14

            Icon {
                name: row.icon
                size: 20
                color: Theme.text
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: row.name
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    text: row.purpose
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }
            AppButton {
                visible: !row.allowed
                enabled: row.available
                text: qsTr("去打开")
                onClicked: row.openRequested()
            }
            Rectangle {
                visible: row.allowed
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 13
                color: Qt.rgba(0.2, 0.78, 0.35, 0.14)
                Icon {
                    anchors.centerIn: parent
                    name: "check"
                    size: 14
                    color: Theme.success
                }
            }
        }
    }

    // 整个窗口按住都能拖
    WindowDragArea { anchors.fill: parent }

    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 6
        width: 440
        spacing: 10

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("让你的其他设备连上这台 Mac")
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 8
            text: win.ready ? qsTr("macOS 要你自己在系统设置里打开这两项") : qsTr("正在启动后台服务…")
            font.pixelSize: 13
            color: Theme.textSecondary
        }

        PermissionRow {
            icon: "monitor"
            name: qsTr("屏幕与系统音频录制")
            purpose: qsTr("让你的其他设备看到、听到这台 Mac")
            allowed: win.ready && IncomingConnections.screenPermission
            available: win.ready
            onOpenRequested: {
                win.asked = true
                IncomingConnections.requestPermission("screen")
            }
        }
        PermissionRow {
            icon: "pointer"
            name: qsTr("辅助功能")
            purpose: qsTr("让你的其他设备操作这台 Mac")
            allowed: win.ready && IncomingConnections.inputPermission
            available: win.ready
            onOpenRequested: {
                win.asked = true
                IncomingConnections.requestPermission("input")
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("打开\"屏幕与系统音频录制\"后，系统可能要求重新打开比邻：重新打开后会回到这里")
            font.pixelSize: 12
            color: Theme.textMuted
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 10
            Layout.preferredWidth: 200
            implicitHeight: 40
            dark: true
            enabled: win.granted
            text: qsTr("完成")
            onClicked: win.finish()
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("以后再说")
            font.pixelSize: 12
            color: Theme.textSecondary
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: win.finish()
            }
        }
    }
}
