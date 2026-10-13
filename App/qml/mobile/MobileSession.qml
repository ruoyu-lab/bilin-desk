import QtQuick
import QtQuick.Layouts

// 手机上连着一台电脑：远程画面铺满（RemoteCanvas，和电脑版的会话一样：连接中、输临时密码、出错都在里面），
// 左上角浮着返回、名字和延迟。返回就断开
Item {
    id: root

    // Devices 里的一项
    required property var device
    property bool viewOnly: false
    property ViewerSession session: null

    signal closed()

    function close() {
        ViewerSessions.close(root.device.key)
        root.closed()
    }

    Component.onCompleted: {
        const session = ViewerSessions.open(device.key, device.target)
        session.resolutionChoice = AppSettings.sessionResolution
        session.frameRateChoice = AppSettings.sessionFrameRate
        session.bitrateChoice = AppSettings.sessionBitrate
        session.codecChoice = AppSettings.sessionCodec
        session.clearText = AppSettings.sessionClearText
        session.lowLatency = AppSettings.sessionLowLatency
        session.audio = AppSettings.remoteAudio
        session.clipboard = AppSettings.clipboardSync
        root.session = session
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    RemoteCanvas {
        anchors.fill: parent
        anchors.topMargin: root.SafeArea.margins.top
        anchors.bottomMargin: root.SafeArea.margins.bottom
        anchors.leftMargin: root.SafeArea.margins.left
        anchors.rightMargin: root.SafeArea.margins.right
        visible: root.session !== null
        session: root.session
        name: root.device.name
        viewOnly: root.viewOnly
        onCloseRequested: root.close()
    }

    // 左上角：返回（断开）、名字、延迟
    Rectangle {
        x: root.SafeArea.margins.left + 10
        y: root.SafeArea.margins.top + 10
        width: pill.implicitWidth + 24
        height: 40
        radius: 20
        color: "#c71c1c1e"

        RowLayout {
            id: pill
            anchors.centerIn: parent
            spacing: 8
            Icon {
                name: "chevron-right"
                rotation: 180
                size: 18
                color: "white"
            }
            Text {
                Layout.maximumWidth: root.width * 0.45
                text: root.device.name
                elide: Text.ElideRight
                font.pixelSize: 14
                color: "white"
            }
            StatusDot {
                visible: root.session !== null
                status: root.session ? root.session.status : ""
            }
            Text {
                visible: root.session !== null && root.session.status === "connected" && root.session.latencyMs > 0
                text: root.session ? qsTr("%1 ms").arg(root.session.latencyMs) : ""
                font.pixelSize: 13
                color: "#d1d1d6"
            }
        }

        TapHandler {
            onTapped: root.close()
        }
    }
}
