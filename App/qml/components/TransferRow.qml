import QtQuick
import QtQuick.Layouts

// 一次文件传输：名字、进度、状态，以及重试（没发完的接着发）/ 取消 / 移除（没收完的删掉收了一半的）/ 在文件夹中显示。
// transfer：files.transfers 里的一项（见 FileTransferList），status 见 rc/transfer_list.h
RowLayout {
    id: root

    required property FileTransferList files
    required property var transfer
    // 现在能不能重试（连着对方）
    property bool canRetry: true

    readonly property string status: transfer.status
    readonly property bool active: status === "preparing" || status === "offered" || status === "running"
                                   || status === "paused" || status === "waiting"
    readonly property bool failed: status === "failed" || status === "rejected" || status === "cancelled"
    readonly property real progress: transfer.totalBytes > 0 ? transfer.doneBytes / transfer.totalBytes : 0

    spacing: 10

    Icon {
        name: root.transfer.sending ? "upload" : "file"
        size: 16
        color: Theme.textSecondary
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: root.transfer.name
            elide: Text.ElideMiddle
            font.pixelSize: 13
            color: Theme.text
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            visible: root.active
            radius: 2
            color: Theme.fill

            Rectangle {
                width: parent.width * root.progress
                height: parent.height
                radius: 2
                color: Theme.accent

                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            }
        }

        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: 12
            color: root.failed ? Theme.danger : Theme.textSecondary
            text: {
                const t = root.transfer
                const size = Theme.formatBytes(t.totalBytes)
                switch (root.status) {
                case "preparing":
                    if (t.totalBytes === 0)
                        return t.sending ? qsTr("准备发送…") : qsTr("准备接收…")
                    return t.sending ? qsTr("准备发送 · %1").arg(size) : qsTr("准备接收 · %1").arg(size)
                case "offered": return qsTr("等对方接收 · %1").arg(size)
                case "running":
                    return qsTr("%1 %2% · %3 / %4 · %5/s").arg(t.sending ? qsTr("发送中") : qsTr("接收中"))
                              .arg(Math.round(root.progress * 100)).arg(Theme.formatBytes(t.doneBytes)).arg(size)
                              .arg(Theme.formatBytes(t.bytesPerSecond))
                case "paused": return qsTr("已暂停 · %1 / %2").arg(Theme.formatBytes(t.doneBytes)).arg(size)
                case "waiting": return qsTr("连接中断，等待对方回来…")
                case "completed":
                    return (t.sending ? qsTr("已发送") : qsTr("已存到“%1”").arg(t.folder)) + " · " + size
                           + (t.message ? " · " + t.message : "")
                default: return t.message
                }
            }
        }
    }

    IconButton {
        visible: root.status === "completed" && !root.transfer.sending && root.transfer.savedPath.length > 0
        implicitWidth: 26
        implicitHeight: 26
        iconSize: 14
        iconName: "folder"
        text: qsTr("在文件夹中显示")
        onClicked: root.files.showInFolder(root.transfer.id)
    }
    Icon {
        visible: root.status === "completed" && root.transfer.sending
        name: "check"
        size: 16
        color: Theme.success
    }
    IconButton {
        visible: root.failed && root.transfer.sending
        enabled: root.canRetry
        implicitWidth: 26
        implicitHeight: 26
        iconSize: 14
        iconName: "refresh"
        text: root.transfer.resumable ? qsTr("接着发") : qsTr("重试")
        onClicked: root.files.retry(root.transfer.id)
    }
    IconButton {
        visible: root.status !== "completed"
        implicitWidth: 26
        implicitHeight: 26
        iconSize: 12
        iconName: "x"
        text: root.active ? qsTr("取消") : qsTr("移除")
        onClicked: root.active ? root.files.cancel(root.transfer.id) : root.files.remove(root.transfer.id)
    }
}
