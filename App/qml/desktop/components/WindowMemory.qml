import QtQuick
import QtCore

// 记住一种窗口（key）的位置、大小、最大化了没有，下次打开还在那里（偏好设置里的 window-<key>）。
// 放在窗口里，窗口自己的 Component.onCompleted 最后调 restore()（在它定好默认的位置以后）：放回记下的位置；
// 挪动、改大小、最大化以后存下来。记下的位置已经不在任何一块屏幕上（那块屏幕拔掉了）时，用窗口默认的位置和大小
QtObject {
    id: root

    required property Window window
    required property string key

    readonly property Settings saved: Settings {
        category: "window-" + root.key
        // 宽为 0：还没记过
        property int x: 0
        property int y: 0
        property int width: 0
        property int height: 0
        property bool maximized: false
    }

    // 最大化、全屏时不记位置和大小（还原以后回到之前那样）；连着改的时候等停下来再存
    readonly property Timer saveLater: Timer {
        interval: 400
        onTriggered: root.store()
    }
    property bool restored: false

    function store() {
        if (!root.restored)
            return
        const w = root.window
        if (w.visibility === Window.Windowed) {
            saved.x = w.x
            saved.y = w.y
            saved.width = w.width
            saved.height = w.height
        }
        if (w.visibility === Window.Windowed || w.visibility === Window.Maximized)
            saved.maximized = w.visibility === Window.Maximized
    }

    // 窗口标题栏那一截（左上角往右 120、往下 40）有一部分落在某块屏幕上，才算看得见、拖得回来
    function onSomeScreen(x, y, width) {
        for (const screen of Qt.application.screens) {
            const left = Math.max(x, screen.virtualX)
            const right = Math.min(x + Math.min(width, 120), screen.virtualX + screen.width)
            const top = Math.max(y, screen.virtualY)
            const bottom = Math.min(y + 40, screen.virtualY + screen.height)
            if (right > left && bottom > top)
                return true
        }
        return false
    }

    function restore() {
        const w = root.window
        if (saved.width > 0 && onSomeScreen(saved.x, saved.y, saved.width)) {
            w.width = Math.max(saved.width, w.minimumWidth)
            w.height = Math.max(saved.height, w.minimumHeight)
            w.x = saved.x
            w.y = saved.y
        }
        root.restored = true
    }

    readonly property Connections watch: Connections {
        target: root.window
        function onXChanged() { root.saveLater.restart() }
        function onYChanged() { root.saveLater.restart() }
        function onWidthChanged() { root.saveLater.restart() }
        function onHeightChanged() { root.saveLater.restart() }
        function onVisibilityChanged() { root.saveLater.restart() }
        // 第一次出现时：上次是最大化的，接着最大化
        function onVisibleChanged() {
            if (root.window.visible && root.saved.maximized && root.window.visibility === Window.Windowed)
                root.window.showMaximized()
        }
        // 关掉时马上存（窗口随后就没了，等不到定时器）
        function onClosing() { root.store() }
    }
}
