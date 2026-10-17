import QtQuick

// 刚进全屏（只有画面，按钮都藏起来了）时，画面上方说几秒怎么退出（RemoteView.fullScreen）
Rectangle {
    id: root

    function flash() {
        opacity = 1
        hide.restart()
    }

    width: label.implicitWidth + 32
    height: 36
    radius: 18
    color: "#c71c1c1e"
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 300 }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: Theme.touch ? qsTr("四个手指按住 3 秒退出全屏")
                          : qsTr("按 %1 退出全屏").arg(Qt.platform.os === "osx" ? "⌃⌥⇧F" : "Ctrl+Alt+Shift+F")
        font.pixelSize: 13
        color: "white"
    }

    Timer {
        id: hide
        interval: 3000
        onTriggered: root.opacity = 0
    }
}
