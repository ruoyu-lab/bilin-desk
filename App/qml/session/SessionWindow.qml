import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 远程会话窗口：每个连接一个会话，标签和工具共用顶部一条细标题栏
ApplicationWindow {
    id: win

    readonly property ListModel sessions: ListModel {}
    property int currentIndex: -1
    // 侧边面板开着没有：记住，下次打开会话窗口还是这样
    property bool panelOpen: AppSettings.sessionPanelOpen
    onPanelOpenChanged: AppSettings.sessionPanelOpen = panelOpen
    readonly property SessionView currentView: views.count > 0 && currentIndex >= 0 ? views.itemAt(currentIndex) : null

    readonly property bool fullScreen: visibility === Window.FullScreen
    readonly property bool keysMenuOpen: keysMenu.visible
    readonly property bool powerMenuOpen: powerMenu.visible || powerConfirm.opened

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

    function indexOfDevice(key) {
        for (let i = 0; i < sessions.count; ++i) {
            if (sessions.get(i).key === key)
                return i
        }
        return -1
    }

    function present() {
        if (!visible)
            show()
        raise()
        requestActivate()
    }

    // 会话本身（和它的对话、互传的文件）在 ViewerSessions 里，按设备各一个；这里只记标签
    function open(device, viewOnly) {
        let index = indexOfDevice(device.key)
        if (index < 0) {
            const session = ViewerSessions.open(device.key, device.target)
            session.resolutionChoice = AppSettings.sessionResolution
            session.frameRateChoice = AppSettings.sessionFrameRate
            session.bitrateChoice = AppSettings.sessionBitrate
            session.codecChoice = AppSettings.sessionCodec
            session.clearText = AppSettings.sessionClearText
            session.lowLatency = AppSettings.sessionLowLatency
            session.audio = AppSettings.remoteAudio
            session.clipboard = AppSettings.clipboardSync
            sessions.append({ key: device.key, deviceId: device.deviceId, name: device.name, address: device.address,
                              os: device.os, viewOnly: viewOnly })
            index = sessions.count - 1
        }
        currentIndex = index
        present()
    }

    // 接收从别的窗口移过来的会话
    function receive(session) {
        sessions.append(session)
        currentIndex = sessions.count - 1
        present()
    }

    // 取出一个会话（并从本窗口移除），用于移到别的窗口；连接不断
    function takeSession(index) {
        const s = sessions.get(index)
        const session = { key: s.key, deviceId: s.deviceId, name: s.name, address: s.address, os: s.os,
                          viewOnly: s.viewOnly }
        removeTab(index)
        return session
    }

    function setViewOnly(viewOnly) {
        sessions.setProperty(currentIndex, "viewOnly", viewOnly)
    }

    // 断开连接，文件也不要了（对话记录还在）
    function disconnect(key) {
        ViewerSessions.close(key)
    }

    // 关闭标签并断开它的连接
    function closeSession(index) {
        disconnect(sessions.get(index).key)
        removeTab(index)
    }

    function removeTab(index) {
        sessions.remove(index)
        if (sessions.count === 0) {
            close()
            return
        }
        if (index < currentIndex || currentIndex >= sessions.count)
            currentIndex--
    }

    function closeOthers(index) {
        for (let i = sessions.count - 1; i >= 0; --i) {
            if (i !== index)
                disconnect(sessions.get(i).key)
        }
        for (let i = sessions.count - 1; i >= 0; --i) {
            if (i !== index)
                sessions.remove(i)
        }
        currentIndex = 0
    }

    function moveSession(from, to) {
        sessions.move(from, to, 1)
        if (currentIndex === from)
            currentIndex = to
        else if (from < currentIndex && currentIndex <= to)
            currentIndex--
        else if (to <= currentIndex && currentIndex < from)
            currentIndex++
    }

    function toggleFullScreen() {
        if (fullScreen)
            showNormal()
        else
            showFullScreen()
    }

    function showKeysMenu(anchor) {
        keysMenu.popup(anchor, 0, anchor.height + 8)
    }

    function showTabMenu(index) {
        tabMenu.target = index
        tabMenu.popup()
    }

    // 右上角的电源按钮：断开连接，或者让对方电脑重启、关机、睡眠、注销（先确认）
    function showPowerMenu(anchor) {
        powerMenu.show(anchor)
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
    onClosing: {
        for (let i = 0; i < sessions.count; ++i)
            disconnect(sessions.get(i).key)
        sessions.clear()
        currentIndex = -1
    }

    Item {
        id: stage
        anchors.fill: parent
        anchors.topMargin: win.compactBarHeight

        Repeater {
            id: views
            model: win.sessions
            delegate: SessionView {
                anchors.fill: parent
                visible: index === win.currentIndex
                panelOpen: win.panelOpen
                onCloseRequested: win.closeSession(index)
            }
        }
    }

    CompactChrome {
        anchors.fill: parent
        session: win
    }

    // 对方系统上的组合键（连上以后按对方是 Mac 还是 Windows）
    AppMenu {
        id: keysMenu
        Repeater {
            model: win.currentView !== null ? win.currentView.shortcuts : []
            delegate: AppMenuItem {
                required property var modelData
                text: modelData.label
                hint: modelData.keys
                enabled: win.currentView !== null && win.currentView.session !== null
                         && win.currentView.session.controlGranted && !win.currentView.viewOnly
                onTriggered: win.currentView.sendShortcut(modelData.id)
            }
        }
    }

    AppMenu {
        id: tabMenu

        property int target: -1
        readonly property bool several: win.sessions.count > 1

        AppMenuItem {
            text: qsTr("复制设备 ID")
            // 按 IP 地址连的没有设备 ID：复制地址
            onTriggered: {
                const s = win.sessions.get(tabMenu.target)
                Clipboard.setText((s.deviceId || s.address).replace(/\s+/g, ""))
            }
        }
        AppMenuItem {
            text: qsTr("在新窗口打开")
            enabled: tabMenu.several
            onTriggered: win.detachRequested(tabMenu.target)
        }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTr("关闭")
            onTriggered: win.closeSession(tabMenu.target)
        }
        AppMenuItem {
            text: qsTr("关闭其他")
            enabled: tabMenu.several
            onTriggered: win.closeOthers(tabMenu.target)
        }
    }

    PowerMenu {
        id: powerMenu
        session: win.currentView !== null ? win.currentView.session : null
        viewOnly: win.currentView !== null && win.currentView.viewOnly
        onDisconnectRequested: win.closeSession(win.currentIndex)
        onActionChosen: action => powerConfirm.openAt(powerMenu.anchor, action)
    }

    PowerConfirm {
        id: powerConfirm
        name: win.currentView !== null ? win.currentView.name : ""
        onConfirmed: action => win.currentView.requestPower(action)
    }
}
