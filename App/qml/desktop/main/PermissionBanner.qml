import QtQuick
import QtQuick.Layouts

// 概览页最上面（OverviewPage 的 banner）：macOS 上被控需要"屏幕与系统音频录制"（画面和声音）和"辅助功能"两个权限，
// 缺的时候说一声（wanted），点了去授权
Rectangle {
    id: permissionBanner

    readonly property bool screenMissing: !IncomingConnections.screenPermission
    readonly property bool inputMissing: !IncomingConnections.inputPermission
    readonly property bool wanted: ThisDevice.serviceState === "ready" && ThisDevice.acceptIncoming
                                   && (screenMissing || inputMissing)

    height: 48
    radius: Theme.cardRadius
    color: Theme.card

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 10
        spacing: 10

        Icon {
            name: "key"
            size: 16
            color: Theme.warning
        }
        Text {
            Layout.fillWidth: true
            text: permissionBanner.screenMissing && permissionBanner.inputMissing
                  ? qsTr("对方要看到、听到并控制本机，需要授予\"屏幕与系统音频录制\"和\"辅助功能\"权限")
                  : permissionBanner.screenMissing ? qsTr("对方要看到、听到本机，需要授予\"屏幕与系统音频录制\"权限")
                  : qsTr("对方要控制本机，需要授予\"辅助功能\"权限")
            elide: Text.ElideRight
            font.pixelSize: 13
            color: Theme.text
        }
        AppButton {
            text: qsTr("去授权")
            onClicked: IncomingConnections.requestPermission(permissionBanner.screenMissing ? "screen" : "input")
        }
    }
}
