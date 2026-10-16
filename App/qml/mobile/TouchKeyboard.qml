pragma ComponentBehavior: Bound

import QtQuick

// 手机、平板上给对方打字、按电脑上的键（MobileSession、TabletSession）：键盘按钮弹出菜单（showMenu），选打字是系统
// 键盘，选按键栏是底下一排电脑上的键（Esc、Tab、Ctrl……，不弹系统键盘、不挡着）。按键栏的修饰键点一下按住，
// 打下一个键时一起按。铺满会话页，target 是正在看的那个画面（RemoteCanvas 或 SessionView：sendText、pressKey、
// focusInput、remoteMac）。接着键盘时不用这个：点一下画面，按键就给对方（RemoteView）
Item {
    id: root

    property var target: null
    // 打字：系统键盘弹出来了（catcher 有焦点）
    readonly property bool typing: catcher.activeFocus
    // 按键栏开着
    property bool keysShown: false
    readonly property bool active: typing || keysShown
    // 系统键盘从哪里开始盖住（Android 报的是物理像素）
    readonly property real keyboardTop: Qt.inputMethod.visible
        ? Math.min(root.height, Qt.inputMethod.keyboardRectangle.y / (Qt.platform.os === "android" ? Screen.devicePixelRatio : 1))
        : root.height
    // 画面下面要让出多高（按键栏、系统键盘）；都没有时为 0
    readonly property real occupied: keysShown ? root.height - keyBar.y : typing ? root.height - keyboardTop : 0
    readonly property bool remoteMac: target !== null && target.remoteMac
    // 按着的修饰键（Control、Alt、Shift、Meta），打下一个键时一起按，按完松开
    property var held: []

    function showMenu(anchor) {
        keyboardMenu.popup(anchor, anchor.width - keyboardMenu.width, anchor.height + 8)
    }

    function startTyping() {
        catcher.forceActiveFocus()
        Qt.inputMethod.show()
    }

    // 收起来：接着的按键（外接键盘）还给画面
    function close() {
        catcher.focus = false
        Qt.inputMethod.hide()
        root.keysShown = false
        if (root.target !== null)
            root.target.focusInput()
    }

    function toggleHeld(modifier) {
        root.held = root.held.includes(modifier) ? root.held.filter(m => m !== modifier) : root.held.concat([modifier])
    }

    function pressKey(key) {
        root.target.pressKey(key, root.held)
        root.held = []
    }

    // 输入法定下来的字：按着修饰键时字母、数字当键按（Ctrl+C），其余的原样发
    function type(text) {
        if (root.held.length > 0 && text.length === 1 && /[A-Za-z0-9]/.test(text)) {
            root.pressKey(text)
            return
        }
        root.target.sendText(text)
        root.held = []
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
                        { modifier: "Control", label: root.remoteMac ? "⌃" : "Ctrl" },
                        { modifier: "Alt", label: root.remoteMac ? "⌥" : "Alt" },
                        { modifier: "Meta", label: root.remoteMac ? "⌘" : "Win" },
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
        // 有焦点时接着的键盘的按键也给对方的画面（iPad，RemoteView::keyTarget），不用先收起打字
        readonly property bool remoteKeys: true
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
}
