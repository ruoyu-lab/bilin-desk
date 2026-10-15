import QtQuick
import QtQuick.Layouts

// 概览页左上角（OverviewPage 的 thisDevice）：本机的设备 ID、临时密码，别人连不上本机时说为什么
Rectangle {
    id: root

    property string copied

    function copy(key, value) {
        Clipboard.setText(value)
        copied = key
        resetCopied.restart()
    }

    radius: Theme.cardRadius
    color: Theme.card

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 4

        Text {
            text: qsTr("本机设备 ID")
            font.pixelSize: 12
            color: Theme.textSecondary
        }
        // 窗口窄了 ID 和复制按钮放不下时，按钮不占位置：鼠标移到这一行上才盖在 ID 右端出现
        Item {
            id: idRow
            readonly property bool tight: idText.implicitWidth + 4 + copyId.implicitWidth > width
            Layout.fillWidth: true
            implicitHeight: Math.max(idText.implicitHeight, copyId.implicitHeight)

            HoverHandler { id: idHover }
            Text {
                id: idText
                anchors.verticalCenter: parent.verticalCenter
                text: ThisDevice.deviceId.length > 0 ? ThisDevice.deviceId : "—"
                font.pixelSize: 26
                font.weight: Font.DemiBold
                font.letterSpacing: 1
                color: Theme.text
            }
            // 盖在 ID 上时垫一层卡片的底色，数字不从图标后面透出来
            Rectangle {
                anchors.fill: copyId
                visible: copyId.visible && idRow.tight
                color: Theme.card
            }
            IconButton {
                id: copyId
                // ID 比这一行还宽（伸进卡片边距）时跟着 ID 的右端，整个盖住最后几个字
                x: Math.max(idRow.width, idText.width) - width
                anchors.verticalCenter: parent.verticalCenter
                visible: ThisDevice.deviceId.length > 0 && (!idRow.tight || idHover.hovered)
                iconName: root.copied === "id" ? "check" : "copy"
                text: qsTr("复制设备 ID")
                onClicked: root.copy("id", ThisDevice.deviceId.replace(/\s+/g, ""))
            }
        }
        Item { Layout.fillHeight: true }
        // 别人现在连不上本机时（后台服务还没连上、起不来，关了别人连接，缺授权，出错了），
        // 临时密码那一行换成一句说明
        RowLayout {
            id: statusRow
            readonly property string serviceState: ThisDevice.serviceState
            readonly property bool failed: serviceState === "unavailable"
                                           || IncomingConnections.problem === "unavailable"
            visible: serviceState !== "ready" || !ThisDevice.acceptIncoming || !IncomingConnections.accepting
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: statusRow.serviceState === "connecting" ? qsTr("正在准备…")
                    : statusRow.serviceState === "unavailable" ? qsTr("后台服务没能启动，本机现在不能被连接")
                    : !ThisDevice.acceptIncoming ? qsTr("已关闭别人连接，可在设置中开启")
                    : IncomingConnections.problem === "permission" ? qsTr("还没授权，别人现在连不上本机")
                    : IncomingConnections.problem === "unavailable" ? qsTr("出错了，别人现在连不上本机")
                    : qsTr("正在准备…")
                elide: Text.ElideRight
                font.pixelSize: 12
                color: statusRow.failed ? Theme.danger : Theme.textMuted
            }
            Text {
                visible: statusRow.failed
                text: qsTr("重试")
                font.pixelSize: 12
                color: Theme.accent
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: statusRow.serviceState === "unavailable" ? ThisDevice.retry()
                                                                        : IncomingConnections.retry()
                }
            }
        }
        RowLayout {
            visible: ThisDevice.serviceState === "ready" && ThisDevice.acceptIncoming && IncomingConnections.accepting
            spacing: 4
            Text {
                text: qsTr("临时密码")
                font.pixelSize: 12
                color: Theme.textSecondary
            }
            Text {
                Layout.leftMargin: 4
                Layout.fillWidth: true
                text: ThisDevice.password
                font.pixelSize: 14
                font.weight: Font.Medium
                font.letterSpacing: 1
                color: Theme.text
            }
            IconButton {
                iconName: "refresh"
                text: qsTr("刷新密码")
                onClicked: ThisDevice.regeneratePassword()
            }
            IconButton {
                iconName: root.copied === "password" ? "check" : "copy"
                text: qsTr("复制临时密码")
                onClicked: root.copy("password", ThisDevice.password)
            }
        }
    }

    Timer {
        id: resetCopied
        interval: 1500
        onTriggered: root.copied = ""
    }
}
