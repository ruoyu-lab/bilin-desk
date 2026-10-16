import QtQuick
import QtQuick.Templates as T

// 一个按钮开关一个弹出的东西（菜单、弹出框）：开着时再点按钮是关掉。
// 弹出的东西点外面就关，按钮按下那一下就先把它关了，松开时按钮又会打开它：所以按下时记住它开着没有
// （刚被这一下关掉的也算开着），松开时 opening 为真才打开。按钮按下时调 press()（IconButton 的 popup 自带）
QtObject {
    id: root

    // 菜单（Menu）、弹出框（Popup）都是 T.Popup
    property T.Popup popup: null
    // 这一下点了该打开（按下时它没开着）
    readonly property bool opening: !wasOpen

    function press() {
        wasOpen = popup !== null && (popup.visible || closing)
    }

    property bool wasOpen: false
    // 刚关掉：同一下按下里按钮还没收到
    property bool closing: false
    readonly property Connections watch: Connections {
        target: root.popup
        function onAboutToHide() {
            root.closing = true
            Qt.callLater(() => root.closing = false)
        }
    }
}
