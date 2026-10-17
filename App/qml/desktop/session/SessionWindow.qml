import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 远程会话窗口：每个连接一个会话，标签和工具共用顶部一条细标题栏
ApplicationWindow {
    id: win

    // 标签和各自的画面、会话的菜单（SessionStage）
    readonly property SessionStage stage: sessionStage
    readonly property SessionView currentView: sessionStage.currentView

    readonly property bool fullScreen: visibility === Window.FullScreen

    // 红绿灯位置由系统决定，据此确定标题栏高度
    property rect titleButtons: Qt.rect(0, 0, 0, 0)
    readonly property int compactBarHeight: !Theme.unifiedTitleBar ? Theme.titleBarHeight
                                          : titleButtons.height > 0 ? Math.round(titleButtons.y * 2 + titleButtons.height) : 34

    // 请求把某个会话移到新窗口，由窗口管理者负责创建
    signal detachRequested(int index)

    width: 1280
    height: 800
    minimumWidth: 820
    minimumHeight: 520
    title: currentView ? currentView.name : qsTr("远程会话")
    color: Theme.canvas
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    function present() {
        if (!visible)
            show()
        raise()
        requestActivate()
    }

    function open(device, viewOnly) {
        sessionStage.open(device, viewOnly)
        present()
    }

    // 接收从别的窗口移过来的会话
    function receive(session) {
        sessionStage.receive(session)
        present()
    }

    // 取出一个会话（并从本窗口移除），用于移到别的窗口；连接不断
    function takeSession(index) {
        return sessionStage.takeSession(index)
    }

    function toggleFullScreen() {
        if (fullScreen)
            showNormal()
        else
            showFullScreen()
    }

    // 位置、大小记下来，下次打开的会话窗口还在那里
    readonly property WindowMemory memory: WindowMemory {
        window: win
        key: "session"
    }

    Component.onCompleted: {
        memory.restore()
        ViewGraphics.prepare(win)
        WindowChrome.applyTitleBar(win, "compact")
        Qt.callLater(() => win.titleButtons = WindowChrome.titleButtonsRect(win))
    }
    onFullScreenChanged: WindowChrome.setToolbarVisible(win, !fullScreen)
    onClosing: sessionStage.closeAll()

    // 全屏只有画面：标签、工具都藏起来，按 Ctrl+Alt+Shift+F 出来（RemoteView.fullScreen）
    SessionStage {
        id: sessionStage
        anchors.fill: parent
        anchors.topMargin: win.fullScreen ? 0 : win.compactBarHeight
        fullScreen: win.fullScreen
        onExitFullScreenRequested: win.showNormal()
        detachable: true
        onDetachRequested: index => win.detachRequested(index)
        onEmptied: win.close()
    }

    CompactChrome {
        anchors.fill: parent
        visible: !win.fullScreen
        session: win
    }

    // 焦点不在画面上时（画面收着组合键时它先拿到，这里收不到）也出得来：Mac 上 Ctrl 是 Command、Meta 是 Control
    Shortcut {
        enabled: win.fullScreen
        sequences: ["Ctrl+Alt+Shift+F", "Meta+Alt+Shift+F"]
        onActivated: win.showNormal()
    }
}
