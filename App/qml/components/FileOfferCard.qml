import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

// 对方发来文件、等本机的人决定：谁想发给你几个文件（多大），接收 / 拒绝。
// 接收的存到设置里选的文件夹（接着上次没发完的存回上次的文件夹）；存不了（文件夹不在了、空间不够）时在这里说明，
// 还等着决定。
// 两分钟没决定 SDK 就当拒绝，卡片随之消失
Rectangle {
    id: root

    required property FileTransferList files
    // files.offers 里的一项
    required property var offer
    // 谁发来的
    required property string from
    // 浮在画面上：白底带阴影；否则是浅灰的底
    property bool elevated: false
    property string error

    implicitHeight: column.implicitHeight + 24
    radius: Theme.cardRadius
    color: elevated ? Theme.surface : Theme.fill

    RectangularShadow {
        visible: root.elevated
        anchors.fill: parent
        // 在卡片底下
        z: -1
        radius: root.radius
        blur: 18
        offset: Qt.vector2d(0, 4)
        color: Qt.rgba(0, 0, 0, 0.14)
    }

    ColumnLayout {
        id: column
        x: 12
        y: 12
        width: parent.width - 24
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Icon {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
                name: "file"
                size: 16
                color: Theme.textSecondary
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.text
                    text: root.offer.resumed
                          ? qsTr("%1 想接着发上次没发完的（还剩 %2）").arg(root.from)
                                .arg(Theme.formatBytes(root.offer.totalBytes - root.offer.doneBytes))
                          : root.offer.files > 0
                          ? qsTr("%1 想发给你 %2 个文件（%3）").arg(root.from).arg(root.offer.files)
                                .arg(Theme.formatBytes(root.offer.totalBytes))
                          : qsTr("%1 想发给你空文件夹").arg(root.from)
                }
                Text {
                    Layout.fillWidth: true
                    text: root.offer.name
                    elide: Text.ElideMiddle
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.error.length > 0
            text: root.error
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Theme.danger
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            AppButton {
                text: qsTr("拒绝")
                onClicked: root.files.reject(root.offer.id)
            }
            AppButton {
                primary: true
                text: qsTr("接收")
                onClicked: root.error = root.files.accept(root.offer.id, AppSettings.downloadFolder)
            }
        }
    }
}
