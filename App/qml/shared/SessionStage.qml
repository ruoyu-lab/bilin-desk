import QtQuick

// 一个窗口里的几个会话（电脑版的会话窗口 SessionWindow、平板的 TabletSession）：每个连接一个标签，同一时间看一个。
// 会话本身（和它的对话、互传的文件）在 ViewerSessions 里，按设备各一个；这里记标签和各自的画面（SessionView），
// 还有会话的几个菜单：快捷键、标签的菜单、电源
Item {
    id: root

    readonly property ListModel sessions: ListModel {}
    property int currentIndex: -1
    // 侧边面板开着没有：记住，下次打开会话还是这样
    property bool panelOpen: AppSettings.sessionPanelOpen
    onPanelOpenChanged: AppSettings.sessionPanelOpen = panelOpen
    readonly property SessionView currentView: views.count > 0 && currentIndex >= 0 ? views.itemAt(currentIndex) as SessionView : null

    readonly property bool keysMenuOpen: keysMenu.visible
    readonly property bool powerMenuOpen: powerMenu.visible || powerConfirm.opened

    // 标签能拆到新窗口（电脑版）
    property bool detachable: false
    // 手机、平板：手指怎么操作对方（RemoteView.touchMode）；电脑上为空
    property string touchMode

    // 请求把某个会话移到新窗口，由窗口管理者负责创建
    signal detachRequested(int index)
    // 最后一个标签关掉了
    signal emptied()
    // 平板：按住不放起了作用（RemoteView）
    signal pressedAndHeld()

    function indexOfDevice(key) {
        for (let i = 0; i < sessions.count; ++i) {
            if (sessions.get(i).key === key)
                return i
        }
        return -1
    }

    // 已经连着的那台切过去，不然新开一个标签
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
        select(index)
    }

    // 换到第 index 个：换走之前存下正在看的那个的缩略图
    function select(index) {
        if (index === currentIndex)
            return
        if (currentView !== null)
            currentView.saveThumbnail()
        currentIndex = index
    }

    // 不看了（关窗口、回到主界面）：存下正在看的那个的缩略图
    function saveThumbnail() {
        if (currentView !== null)
            currentView.saveThumbnail()
    }

    // 接收从别的窗口移过来的会话
    function receive(session) {
        sessions.append(session)
        currentIndex = sessions.count - 1
    }

    // 取出一个会话（并从这里移除），用于移到别的窗口；连接不断
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
        if (index === currentIndex)
            saveThumbnail()
        disconnect(sessions.get(index).key)
        removeTab(index)
    }

    // 都断开（窗口关掉了）
    function closeAll() {
        saveThumbnail()
        for (let i = 0; i < sessions.count; ++i)
            disconnect(sessions.get(i).key)
        sessions.clear()
        currentIndex = -1
    }

    function removeTab(index) {
        sessions.remove(index)
        if (sessions.count === 0) {
            currentIndex = -1
            emptied()
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

    function showKeysMenu(anchor) {
        keysMenu.popup(anchor, 0, anchor.height + 8)
    }

    // 第 index 个标签的菜单，出在 item 的 (x, y)
    function showTabMenu(index, item, x, y) {
        tabMenu.target = index
        tabMenu.popup(item, x, y)
    }

    // 电源按钮：断开连接，或者让对方电脑重启、关机、睡眠、注销（先确认）
    function showPowerMenu(anchor) {
        powerMenu.show(anchor)
    }

    Repeater {
        id: views
        model: root.sessions
        delegate: SessionView {
            anchors.fill: parent
            visible: index === root.currentIndex
            panelOpen: root.panelOpen
            touchMode: root.touchMode
            onCloseRequested: root.closeSession(index)
            onPressedAndHeld: root.pressedAndHeld()
        }
    }

    // 对方系统上的组合键（连上以后按对方是 Mac 还是 Windows）
    AppMenu {
        id: keysMenu
        Repeater {
            model: root.currentView !== null ? root.currentView.shortcuts : []
            delegate: AppMenuItem {
                required property var modelData
                text: modelData.label
                hint: modelData.keys
                enabled: root.currentView !== null && root.currentView.session !== null
                         && root.currentView.session.controlGranted && !root.currentView.viewOnly
                onTriggered: root.currentView.sendShortcut(modelData.id)
            }
        }
    }

    AppMenu {
        id: tabMenu

        property int target: -1
        readonly property bool several: root.sessions.count > 1

        AppMenuItem {
            text: qsTr("复制设备 ID")
            // 按 IP 地址连的没有设备 ID：复制地址
            onTriggered: {
                const s = root.sessions.get(tabMenu.target)
                Clipboard.setText((s.deviceId || s.address).replace(/\s+/g, ""))
            }
        }
        AppMenuItem {
            // 菜单里藏起来的一项也占着地方：高度跟着清零
            visible: root.detachable
            height: visible ? implicitHeight : 0
            text: qsTr("在新窗口打开")
            enabled: tabMenu.several
            onTriggered: root.detachRequested(tabMenu.target)
        }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTr("关闭")
            onTriggered: root.closeSession(tabMenu.target)
        }
        AppMenuItem {
            text: qsTr("关闭其他")
            enabled: tabMenu.several
            onTriggered: root.closeOthers(tabMenu.target)
        }
    }

    PowerMenu {
        id: powerMenu
        session: root.currentView !== null ? root.currentView.session : null
        viewOnly: root.currentView !== null && root.currentView.viewOnly
        onDisconnectRequested: root.closeSession(root.currentIndex)
        onActionChosen: action => powerConfirm.openAt(powerMenu.anchor, action)
    }

    PowerConfirm {
        id: powerConfirm
        name: root.currentView !== null ? root.currentView.name : ""
        onConfirmed: action => root.currentView.requestPower(action)
    }
}
