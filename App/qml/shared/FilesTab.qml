import QtQuick
import QtQuick.Layouts

// 远程窗口侧边面板的"文件"：拖进来或选择文件、文件夹发给对方；对方发来的每次都问，
// 收下的存到设置里选的文件夹（默认"下载"）
ColumnLayout {
    id: root

    required property ViewerSession session
    // 对方的名字
    required property string name

    readonly property ViewerFiles files: session ? session.files : null
    readonly property var transfers: files ? files.transfers : []
    readonly property var offers: files ? files.offers : []
    readonly property bool connected: session !== null && session.status === "connected"
    readonly property bool hasCompleted: transfers.some(t => t.status === "completed")
    property string error

    function send(urls) {
        error = root.files.send(urls)
    }

    // Android：要发的文件在后台复制，复制不了时
    Connections {
        target: root.files
        function onSendFailed(message) { root.error = message }
    }

    spacing: 12

    // 对方发来的，等本机的人决定
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 14
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        visible: root.offers.length > 0
        spacing: 8

        Repeater {
            model: root.offers
            delegate: FileOfferCard {
                required property var modelData
                Layout.fillWidth: true
                files: root.files
                offer: modelData
                from: root.name
            }
        }
    }

    DropArea {
        id: dropArea
        Layout.fillWidth: true
        Layout.topMargin: root.offers.length > 0 ? 0 : 14
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        implicitHeight: 110
        enabled: root.connected
        keys: ["text/uri-list"]
        onDropped: drop => root.send(drop.urls)

        Rectangle {
            anchors.fill: parent
            radius: Theme.cardRadius
            color: dropArea.containsDrag ? Theme.selection : Theme.fill

            Column {
                anchors.centerIn: parent
                spacing: 8
                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: "upload"
                    size: 22
                    color: dropArea.containsDrag ? Theme.accent : Theme.textSecondary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.connected ? qsTr("拖拽文件或文件夹到这里发送") : qsTr("连接后可以发送文件")
                    font.pixelSize: Theme.captionFont
                    color: Theme.textSecondary
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        visible: root.error.length > 0
        text: qsTr("没有发出去：%1").arg(root.error)
        wrapMode: Text.Wrap
        font.pixelSize: Theme.captionFont
        color: Theme.danger
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14

        Text {
            Layout.fillWidth: true
            text: qsTr("传输列表")
            font.pixelSize: Theme.captionFont
            font.weight: Font.Medium
            color: Theme.textMuted
        }
        Text {
            visible: root.hasCompleted
            text: qsTr("清除已完成")
            font.pixelSize: Theme.captionFont
            color: clearArea.containsMouse ? Theme.text : Theme.textSecondary

            MouseArea {
                id: clearArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.files.clearCompleted()
            }
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 14
        Layout.rightMargin: 14
        spacing: 12
        clip: true
        // 按位置取：传输中进度一秒变几次，列表不跟着重建
        model: root.transfers.length

        Text {
            parent: list
            anchors.centerIn: parent
            visible: list.count === 0
            text: qsTr("暂无传输")
            font.pixelSize: Theme.captionFont
            color: Theme.textMuted
        }

        delegate: TransferRow {
            required property int index
            width: ListView.view.width
            files: root.files
            transfer: root.transfers[index]
            canRetry: root.connected
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: 14
        spacing: 8

        AppButton {
            Layout.fillWidth: true
            enabled: root.connected
            iconName: "file"
            text: qsTr("发送文件")
            onClicked: pickers.chooseFiles()
        }
        // Android 的文件选择器给的是整个文件夹的授权地址，复制不了，先不支持
        AppButton {
            visible: Qt.platform.os !== "android"
            Layout.fillWidth: true
            enabled: root.connected
            iconName: "folder"
            text: qsTr("发送文件夹")
            onClicked: pickers.chooseFolder()
        }
    }

    // 系统的选文件对话框（电脑上 desktop/session/FilePickers.qml，Android 上 android/FilePickers.qml）
    FilePickers {
        id: pickers
        onChosen: urls => root.send(urls)
    }
}
