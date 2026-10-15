import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 电脑版主窗口工具栏上的通知（MainToolbar 的 homeTools）：本机的连接记录、程序更新了，有没看过的带个红点
ToolGroup {
    id: root

    // 弹出的浮窗按工具栏的规矩放（MainToolbar.placeBelow）
    required property MainToolbar toolbar

    IconButton {
        id: bellButton
        compact: true
        iconName: "bell"
        text: qsTr("通知")
        onClicked: {
            root.toolbar.placeBelow(notifications, bellButton)
            notifications.open()
        }

        // 有没看过的连接记录
        Rectangle {
            visible: IncomingConnections.history.unread > 0
            x: parent.width - width - 5
            y: 5
            width: 7
            height: 7
            radius: 3.5
            color: Theme.danger
        }
    }

    // 通知：本机的连接记录（IncomingConnections.history），还有程序自己更新了，打开就算看过了
    Popup {
        id: notifications

        readonly property var entries: IncomingConnections.history.entries

        // 一条记录说的话
        function describe(entry) {
            const who = entry.name.length > 0 ? entry.name : entry.deviceId
            switch (entry.kind) {
            case "connection": {
                const how = entry.control ? qsTr("控制") : qsTr("查看")
                if (!entry.endedAt)
                    return qsTr("%1 正在%2本机").arg(who).arg(how)
                return qsTr("%1 %2了本机 %3").arg(who).arg(how).arg(duration(entry.endedAt - entry.at))
            }
            case "refused":
                return entry.reason === "refused" ? qsTr("拒绝了 %1 的连接请求").arg(who)
                     : entry.reason === "timeout" ? qsTr("%1 请求连接本机，一直没人允许").arg(who)
                     : qsTr("%1 请求连接本机，那时没开着本程序").arg(who)
            case "wrongPin":
                return qsTr("有人输错了临时密码")
            case "file": {
                const what = entry.count > 1 ? qsTr("%1 等 %2 项").arg(entry.file).arg(entry.count) : entry.file
                return entry.sent ? qsTr("发给 %1：%2").arg(who).arg(what) : qsTr("收到 %1 发来的 %2").arg(who).arg(what)
            }
            case "updated":
                return qsTr("程序更新到了 %1").arg(entry.version)
            case "power":
                switch (entry.action) {
                case "restart": return qsTr("%1 远程重启了本机").arg(who)
                case "shutDown": return qsTr("%1 远程关闭了本机").arg(who)
                case "sleep": return qsTr("%1 让本机睡眠了").arg(who)
                case "signOut": return qsTr("%1 远程注销了本机的用户").arg(who)
                }
                return ""
            }
            return ""
        }

        function duration(ms) {
            const minutes = Math.round(ms / 60000)
            if (minutes < 1)
                return qsTr("不到 1 分钟")
            if (minutes < 60)
                return qsTr("%1 分钟").arg(minutes)
            return qsTr("%1 小时 %2 分钟").arg(Math.floor(minutes / 60)).arg(minutes % 60)
        }

        // 今天的只写时间，昨天的写"昨天"，再早写日期
        function when(date) {
            const today = new Date()
            const day = new Date(date.getFullYear(), date.getMonth(), date.getDate())
            const days = Math.round((new Date(today.getFullYear(), today.getMonth(), today.getDate()) - day) / 86400000)
            const time = Qt.formatTime(date, "hh:mm")
            if (days === 0)
                return time
            if (days === 1)
                return qsTr("昨天 %1").arg(time)
            return Qt.formatDate(date, today.getFullYear() === date.getFullYear() ? "M月d日" : "yyyy年M月d日") + " " + time
        }

        width: 320
        padding: 16
        background: PopupSurface {}
        onOpened: IncomingConnections.history.markSeen()

        contentItem: ColumnLayout {
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("通知")
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.text
                }
                Text {
                    visible: notifications.entries.length > 0
                    text: qsTr("清空")
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: IncomingConnections.history.clear()
                    }
                }
            }
            Text {
                visible: notifications.entries.length === 0
                text: qsTr("谁连过本机、互传的文件、程序的更新会记在这里")
                font.pixelSize: 13
                color: Theme.textMuted
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 360)
                visible: notifications.entries.length > 0
                clip: true
                spacing: 10
                boundsBehavior: Flickable.StopAtBounds
                model: notifications.entries
                ScrollBar.vertical: ScrollBar {}
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: notifications.describe(modelData)
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        color: modelData.kind === "wrongPin" ? Theme.danger : Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        text: notifications.when(modelData.at)
                              + (modelData.deviceId.length > 0 ? " · " + modelData.deviceId : "")
                        font.pixelSize: 11
                        color: Theme.textMuted
                    }
                }
            }
        }
    }
}
