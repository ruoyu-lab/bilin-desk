pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 手机上连着一台电脑：远程画面铺满（RemoteCanvas，和电脑版的会话一样：连接中、输临时密码、出错都在里面），
// 上面浮着几个按钮：左上角名字和延迟（点开是断开连接、对方电脑的重启关机，PowerMenu），右上角显示器（对方有
// 几块时）、键盘、快捷键、手指操作方式、全屏（只有画面，四个手指按住 3 秒出来）、"…"；放大了时中间是倍数，点一下复原。系统的返回先问一句再断开。
// 键盘按钮弹出菜单：打字（系统键盘），或者按键栏（底下一排电脑上的键：Esc、Tab、Ctrl……，不弹系统键盘、
// 不挡着），按键栏的修饰键点一下按住、打下一个键时一起按。
// "…"拉出电脑版会话的侧边面板（SidePanel：对话、文件、设置）：竖着从下面拉出来，横着在右边
Item {
    id: root

    // Devices 里的一项
    required property var device
    property bool viewOnly: false
    // 手指怎么操作：touchpad | direct（MobileMain 按设置和手机、平板定）
    property string fingerMode: "touchpad"
    property ViewerSession session: null
    // 侧边面板（SidePanel）要的：开着没有、名字、是不是原始大小（手机上用手指缩放，一直是适应）
    property bool panelOpen: false
    // 面板关了：按键还给画面
    onPanelOpenChanged: {
        if (!panelOpen)
            canvas.focusInput()
    }
    readonly property string name: device.name
    property bool nativeSize: false

    signal closed()

    readonly property bool controlling: canvas.controlling && canvas.showVideo
    // 全屏：只有画面，浮着的按钮、系统的状态栏都藏起来；四个手指按住 3 秒出来（RemoteView.fullScreen）
    property bool fullScreen: false
    // 进全屏之前窗口是什么样，出来时回到它
    property int windowVisibility: Window.AutomaticVisibility
    onFullScreenChanged: {
        const window = root.Window.window
        if (fullScreen) {
            keyboard.close()
            windowVisibility = window.visibility
            window.visibility = Window.FullScreen
            fullScreenHint.flash()
        } else {
            window.visibility = windowVisibility
        }
    }
    Component.onDestruction: {
        if (fullScreen && root.Window.window)
            root.Window.window.visibility = windowVisibility
    }

    function close() {
        ViewerSessions.close(root.device.key)
        root.closed()
    }

    // 系统的返回（MobileMain）：先问一句，免得手一滑就断了
    function askDisconnect() {
        powerConfirm.openCentered("disconnect")
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

    // 连着的时候屏幕不自己熄灭（设置里能关）
    Binding {
        target: PhoneWindow
        property: "keepAwake"
        value: AppSettings.keepScreenOn && root.session !== null
    }
    // 控制着的时候接着的键盘按 Win+R、Alt+Tab 也给对方，不给手机
    Binding {
        target: PhoneWindow
        property: "captureKeys"
        value: root.controlling
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    RemoteCanvas {
        id: canvas
        anchors.fill: parent
        anchors.topMargin: root.SafeArea.margins.top
        anchors.leftMargin: root.SafeArea.margins.left
        anchors.rightMargin: root.SafeArea.margins.right
        // 让开按键栏、系统键盘
        anchors.bottomMargin: keyboard.active ? keyboard.occupied : root.SafeArea.margins.bottom
        visible: root.session !== null
        session: root.session
        name: root.device.name
        viewOnly: root.viewOnly
        touchMode: root.fingerMode
        // 状态条在上面那排按钮下面
        statusTop: root.fullScreen ? 12 : 58
        fullScreen: root.fullScreen
        onExitFullScreenRequested: root.fullScreen = false
        onCloseRequested: root.close()
        // 按住不放出了右键、开始拖：震一下
        onPressedAndHeld: PhoneWindow.pressFeedback()
    }

    // 画面上浮着的一组：深色半透明的圆角条
    component Pill: Rectangle {
        height: 40
        radius: 20
        color: "#c71c1c1e"
    }

    // ---- 左上角：名字、延迟；点开是断开连接和对方电脑的电源 ----
    // 最宽到右上角那一组前面（横屏、名字长的时候）：名字只显示前面一段
    Pill {
        id: namePill
        visible: !root.fullScreen
        readonly property real rightEdge: panelPill.visible ? panelPill.x
                                          : toolsPill.visible ? toolsPill.x : root.width - root.SafeArea.margins.right - 2
        x: root.SafeArea.margins.left + 10
        y: root.SafeArea.margins.top + 10
        width: Math.min(back.implicitWidth + 24, rightEdge - 8 - x)

        RowLayout {
            id: back
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            Icon {
                name: "chevron-right"
                rotation: 180
                size: 18
                color: "white"
            }
            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth
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

        // 开着时再点是关掉（PopupToggle）
        TapHandler {
            onPressedChanged: if (pressed) powerToggle.press()
            onTapped: {
                if (!powerToggle.opening)
                    return
                powerMenu.anchor = namePill
                powerMenu.popup(namePill, 0, namePill.height + 8)
            }
        }
        PopupToggle {
            id: powerToggle
            popup: powerMenu
        }
    }

    PowerMenu {
        id: powerMenu
        session: root.session
        viewOnly: root.viewOnly
        onDisconnectRequested: root.close()
        onActionChosen: action => powerConfirm.openCentered(action)
    }

    PowerConfirm {
        id: powerConfirm
        name: root.device.name
        onConfirmed: action => action === "disconnect" ? root.close() : root.session.requestPower(action)
    }

    // 对方系统上的组合键（Ctrl+Alt+Del、Win+D……，按对方是 Mac 还是 Windows）
    AppMenu {
        id: keysMenu
        Repeater {
            model: canvas.shortcuts
            delegate: AppMenuItem {
                required property var modelData
                text: modelData.label
                hint: modelData.keys
                onTriggered: canvas.sendShortcut(modelData.id)
            }
        }
    }


    // 对方的几块显示器：一块一张缩略图，点哪块看哪块（同电脑版）
    DisplayThumbnails {
        id: displayThumbnails
        parent: Overlay.overlay
        displays: root.session !== null ? root.session.displays : null
        canChoose: root.controlling
    }

    // ---- 右上角：显示器（对方有几块时）、键盘、快捷键、手指操作方式（控制着的时候才有） ----
    Pill {
        id: toolsPill
        visible: root.controlling && !root.fullScreen
        anchors.right: parent.right
        anchors.rightMargin: root.SafeArea.margins.right + 10
        y: root.SafeArea.margins.top + 10
        width: tools.implicitWidth + 16

        RowLayout {
            id: tools
            anchors.centerIn: parent
            spacing: 0

            Item {
                id: displaysButton
                visible: displayThumbnails.count > 1
                Layout.preferredWidth: 44
                Layout.preferredHeight: 40
                Icon {
                    anchors.centerIn: parent
                    name: "monitor"
                    size: 20
                    color: displayThumbnails.opened ? "#0a84ff" : "white"
                }
                TapHandler {
                    onPressedChanged: if (pressed) displaysToggle.press()
                    onTapped: if (displaysToggle.opening) displayThumbnails.show(displaysButton)
                }
                PopupToggle {
                    id: displaysToggle
                    popup: displayThumbnails
                }
            }
            Item {
                id: keyboardButton
                Layout.preferredWidth: 44
                Layout.preferredHeight: 40
                Icon {
                    anchors.centerIn: parent
                    name: "keyboard"
                    size: 20
                    color: keyboard.active ? "#0a84ff" : "white"
                }
                // 开着的时候点一下收起
                TapHandler {
                    onPressedChanged: if (pressed) keyboardToggle.press()
                    onTapped: {
                        if (keyboard.active)
                            keyboard.close()
                        else if (keyboardToggle.opening)
                            keyboard.showMenu(keyboardButton)
                    }
                }
                PopupToggle {
                    id: keyboardToggle
                    popup: keyboard.menu
                }
            }
            Item {
                id: keysButton
                Layout.preferredWidth: 44
                Layout.preferredHeight: 40
                Icon {
                    anchors.centerIn: parent
                    name: "command"
                    size: 19
                    color: "white"
                }
                TapHandler {
                    onPressedChanged: if (pressed) keysToggle.press()
                    onTapped: {
                        if (keysToggle.opening)
                            keysMenu.popup(keysButton, keysButton.width - keysMenu.width, keysButton.height + 8)
                    }
                }
                PopupToggle {
                    id: keysToggle
                    popup: keysMenu
                }
            }
            Item {
                Layout.preferredWidth: mode.implicitWidth + 20
                Layout.preferredHeight: 40
                Text {
                    id: mode
                    anchors.centerIn: parent
                    text: root.fingerMode === "touchpad" ? qsTr("触摸板") : qsTr("直接点")
                    font.pixelSize: 14
                    color: "white"
                }
                TapHandler {
                    onTapped: AppSettings.fingerMode = root.fingerMode === "touchpad" ? "direct" : "touchpad"
                }
            }
        }
    }

    // 右上角最后一组（连上以后，只看时也有）：全屏；"…"拉出面板，有没看的消息时带个红点
    Pill {
        id: panelPill
        visible: root.session !== null && !root.fullScreen
        anchors.right: parent.right
        anchors.rightMargin: root.SafeArea.margins.right + 10 + (toolsPill.visible ? toolsPill.width + 8 : 0)
        y: root.SafeArea.margins.top + 10
        width: 88

        Item {
            width: 44
            height: parent.height
            Icon {
                anchors.centerIn: parent
                name: "maximize"
                size: 19
                color: "white"
            }
            TapHandler {
                onTapped: root.fullScreen = true
            }
        }
        Item {
            x: 44
            width: 44
            height: parent.height
            Icon {
                anchors.centerIn: parent
                name: "panel-right"
                size: 20
                color: "white"
            }
            Rectangle {
                x: parent.width - 14
                y: 8
                width: 8
                height: 8
                radius: 4
                color: Theme.danger
                visible: root.session !== null && root.session.chat.conversation.unread > 0
            }
            TapHandler {
                onTapped: {
                    keyboard.close()
                    root.panelOpen = true
                }
            }
        }
    }

    FullScreenHint {
        id: fullScreenHint
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.SafeArea.margins.top + 12
    }

    // ---- 放大了：倍数，点一下复原（上面那排按钮下面，和状态条一个位置，有状态条时不显示） ----
    Pill {
        visible: canvas.zoom > 1.01 && !canvas.statusShown && !root.fullScreen
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.SafeArea.margins.top + 58
        width: zoomText.implicitWidth + 28
        height: 32
        radius: 16
        Text {
            id: zoomText
            anchors.centerIn: parent
            text: qsTr("%1%").arg(Math.round(canvas.zoom * 100))
            font.pixelSize: 13
            color: "white"
        }
        TapHandler {
            onTapped: canvas.resetZoom()
        }
    }

    // ---- 打字、按键栏 ----
    TouchKeyboard {
        id: keyboard
        anchors.fill: parent
        target: canvas
    }

    // ---- 面板：竖着从下面拉出来，横着在右边；点外面关掉 ----
    Rectangle {
        anchors.fill: parent
        visible: root.panelOpen
        color: Theme.scrim
        TapHandler {
            onTapped: root.panelOpen = false
        }
    }

    Rectangle {
        id: sheet
        readonly property bool side: root.width > root.height
        visible: root.panelOpen
        width: side ? Math.min(400, root.width * 0.5) + root.SafeArea.margins.right : root.width
        height: side ? root.height : Math.round(root.height * 0.62)
        x: root.width - width
        y: root.height - height
        radius: side ? 0 : 16
        color: Theme.surface

        // 竖着时上面的小横条（看得出能拉、点一下关上）
        Rectangle {
            visible: !sheet.side
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            width: 36
            height: 5
            radius: 2.5
            color: Theme.fillHover
            TapHandler {
                margin: 16
                onTapped: root.panelOpen = false
            }
        }

        SidePanel {
            anchors.fill: parent
            anchors.topMargin: sheet.side ? root.SafeArea.margins.top + 8 : 20
            anchors.bottomMargin: root.SafeArea.margins.bottom
            anchors.rightMargin: sheet.side ? root.SafeArea.margins.right : 0
            view: root
            color: "transparent"
        }
    }
}
