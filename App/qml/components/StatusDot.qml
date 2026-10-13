import QtQuick

// 会话状态点：连接中、重连中会呼吸闪烁，已连接和失败保持常亮
Rectangle {
    id: root

    property string status

    readonly property bool busy: status === "connecting" || status === "waiting" || status === "reconnecting"

    implicitWidth: 6
    implicitHeight: 6
    radius: width / 2
    color: Theme.statusColor(status)

    Behavior on color { ColorAnimation { duration: 150 } }

    SequentialAnimation on opacity {
        running: root.busy
        loops: Animation.Infinite
        NumberAnimation { to: 0.25; duration: 650; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1; duration: 650; easing.type: Easing.InOutSine }
    }
    onBusyChanged: if (!busy) opacity = 1
}
