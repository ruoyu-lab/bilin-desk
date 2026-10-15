pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// 手机上连着一台电脑：远程画面铺满（RemoteCanvas，和电脑版的会话一样：连接中、输临时密码、出错都在里面），
// 上面浮着几个按钮：左上角名字和延迟（点开是断开连接、对方电脑的重启关机，PowerMenu），右上角键盘、快捷键、
// 手指操作方式、"…"；放大了时中间是倍数，点一下复原。系统的返回先问一句再断开。
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
    readonly property string name: device.name
    property bool nativeSize: false

    signal closed()

    readonly property bool controlling: canvas.controlling && canvas.showVideo
    // 打字：系统键盘弹出来了（catcher 有焦点）
    readonly property bool typing: catcher.activeFocus
    // 按键栏开着
    property bool keysShown: false
    // 系统键盘从哪里开始盖住（Android 报的是物理像素）
    readonly property real keyboardTop: Qt.inputMethod.visible
        ? Math.min(root.height, Qt.inputMethod.keyboardRectangle.y / (Qt.platform.os === "android" ? Screen.devicePixelRatio : 1))
        : root.height
    // 按着的修饰键（Control、Alt、Shift、Meta），打下一个键时一起按，按完松开
    property var held: []

    function close() {
        ViewerSessions.close(root.device.key)
        root.closed()
    }

    // 系统的返回（MobileMain）：先问一句，免得手一滑就断了
    function askDisconnect() {
        powerConfirm.openCentered("disconnect")
    }

    function startTyping() {
        catcher.forceActiveFocus()
        Qt.inputMethod.show()
    }

    function closeKeyboard() {
        catcher.focus = false
        Qt.inputMethod.hide()
        root.keysShown = false
    }

    function toggleHeld(modifier) {
        root.held = root.held.includes(modifier) ? root.held.filter(m => m !== modifier) : root.held.concat([modifier])
    }

    function pressKey(key) {
        canvas.pressKey(key, root.held)
        root.held = []
    }

    // 输入法定下来的字：按着修饰键时字母、数字当键按（Ctrl+C），其余的原样发
    function type(text) {
        if (root.held.length > 0 && text.length === 1 && /[A-Za-z0-9]/.test(text)) {
            root.pressKey(text)
            return
        }
        canvas.sendText(text)
        root.held = []
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
        anchors.bottomMargin: root.keysShown ? root.height - keyBar.y
                              : root.typing ? root.height - root.keyboardTop : root.SafeArea.margins.bottom
        visible: root.session !== null
        session: root.session
        name: root.device.name
        viewOnly: root.viewOnly
        touchMode: root.fingerMode
        // 状态条在上面那排按钮下面
        statusTop: 58
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

        TapHandler {
            onTapped: {
                powerMenu.anchor = namePill
                powerMenu.popup(namePill, 0, namePill.height + 8)
            }
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


    // 键盘按钮：打字用系统键盘，或者只要电脑上的那些键
    AppMenu {
        id: keyboardMenu
        AppMenuItem {
            text: qsTr("打字")
            hint: qsTr("系统键盘")
            // 菜单关掉以后再拿焦点（关的时候焦点回到原来的地方）
            onTriggered: Qt.callLater(root.startTyping)
        }
        AppMenuItem {
            text: qsTr("按键栏")
            hint: qsTr("Esc、Ctrl、方向键、F1–F12")
            onTriggered: root.keysShown = true
        }
    }

    // ---- 右上角：键盘、快捷键、手指操作方式（控制着的时候才有） ----
    Pill {
        id: toolsPill
        visible: root.controlling
        anchors.right: parent.right
        anchors.rightMargin: root.SafeArea.margins.right + 10
        y: root.SafeArea.margins.top + 10
        width: tools.implicitWidth + 16

        RowLayout {
            id: tools
            anchors.centerIn: parent
            spacing: 0

            Item {
                id: keyboardButton
                Layout.preferredWidth: 44
                Layout.preferredHeight: 40
                Icon {
                    anchors.centerIn: parent
                    name: "keyboard"
                    size: 20
                    color: root.typing || root.keysShown ? "#0a84ff" : "white"
                }
                // 开着的时候点一下收起
                TapHandler {
                    onTapped: root.typing || root.keysShown
                              ? root.closeKeyboard()
                              : keyboardMenu.popup(keyboardButton, keyboardButton.width - keyboardMenu.width,
                                                   keyboardButton.height + 8)
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
                    onTapped: keysMenu.popup(keysButton, keysButton.width - keysMenu.width, keysButton.height + 8)
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

    // 右上角最后一个："…"（连上以后，只看时也有），拉出面板；有没看的消息时带个红点
    Pill {
        id: panelPill
        visible: root.session !== null
        anchors.right: parent.right
        anchors.rightMargin: root.SafeArea.margins.right + 10 + (root.controlling ? toolsPill.width + 8 : 0)
        y: root.SafeArea.margins.top + 10
        width: 44
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
                root.closeKeyboard()
                root.panelOpen = true
            }
        }
    }

    // ---- 放大了：倍数，点一下复原（上面那排按钮下面，和状态条一个位置，有状态条时不显示） ----
    Pill {
        visible: canvas.zoom > 1.01 && !canvas.statusShown
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

    // ---- 按键栏：在底下 ----
    Rectangle {
        id: keyBar
        visible: root.keysShown
        width: parent.width
        height: 48 + root.SafeArea.margins.bottom
        y: root.height - height
        color: Theme.dark ? "#2c2c2e" : "#d1d3d9"

        Flickable {
            anchors.fill: parent
            anchors.leftMargin: root.SafeArea.margins.left + 6
            anchors.rightMargin: root.SafeArea.margins.right + 6
            anchors.bottomMargin: root.SafeArea.margins.bottom
            contentWidth: keys.implicitWidth
            flickableDirection: Flickable.HorizontalFlick
            clip: true

            Row {
                id: keys
                height: parent.height
                spacing: 6

                Repeater {
                    model: [
                        { key: "Escape", label: "Esc" },
                        { key: "Tab", label: "Tab" },
                        { modifier: "Control", label: canvas.remoteMac ? "⌃" : "Ctrl" },
                        { modifier: "Alt", label: canvas.remoteMac ? "⌥" : "Alt" },
                        { modifier: "Meta", label: canvas.remoteMac ? "⌘" : "Win" },
                        { modifier: "Shift", label: "⇧" },
                        { key: "Left", label: "←" },
                        { key: "Up", label: "↑" },
                        { key: "Down", label: "↓" },
                        { key: "Right", label: "→" },
                        { key: "Delete", label: "Del" },
                        { key: "Home", label: "Home" },
                        { key: "End", label: "End" },
                        { key: "PageUp", label: "PgUp" },
                        { key: "PageDown", label: "PgDn" },
                        { key: "F1", label: "F1" }, { key: "F2", label: "F2" }, { key: "F3", label: "F3" },
                        { key: "F4", label: "F4" }, { key: "F5", label: "F5" }, { key: "F6", label: "F6" },
                        { key: "F7", label: "F7" }, { key: "F8", label: "F8" }, { key: "F9", label: "F9" },
                        { key: "F10", label: "F10" }, { key: "F11", label: "F11" }, { key: "F12", label: "F12" }
                    ]
                    delegate: Rectangle {
                        id: keyButton
                        required property var modelData
                        readonly property bool on: modelData.modifier !== undefined && root.held.includes(modelData.modifier)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(44, label.implicitWidth + 20)
                        height: 36
                        radius: 7
                        color: on ? Theme.text : press.pressed ? Theme.fillHover : Theme.surface

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: keyButton.modelData.label
                            font.pixelSize: 14
                            color: keyButton.on ? Theme.bg : Theme.text
                        }
                        TapHandler {
                            id: press
                            onTapped: keyButton.modelData.modifier !== undefined ? root.toggleHeld(keyButton.modelData.modifier)
                                                                                 : root.pressKey(keyButton.modelData.key)
                        }
                    }
                }
            }
        }
    }

    // ---- 接住输入法打的字：看不见，键盘按钮让它拿焦点 ----
    // 里面一直留一个零宽空格：打的字跟在它后面；它被删掉就是按了退格
    TextInput {
        id: catcher
        readonly property string sentinel: "​"
        x: -width - 10
        width: 10
        height: 10
        text: sentinel
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        onTextEdited: {
            if (text.length < sentinel.length) {
                root.pressKey("Backspace")
            } else {
                const typed = text.replace(sentinel, "")
                if (typed.length > 0)
                    root.type(typed)
            }
            text = sentinel
            cursorPosition = text.length
        }
        Keys.onReturnPressed: root.pressKey("Enter")
        Keys.onEnterPressed: root.pressKey("Enter")
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
