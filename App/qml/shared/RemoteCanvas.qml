import QtQuick
import QtQuick.Layouts

// 一个会话的画面区域：连上以后是远程画面，之前和出错时是居中的状态说明；
// 需要密码时在这里输入
Rectangle {
    id: root

    required property ViewerSession session
    property string name
    property bool viewOnly: false
    property bool nativeSize: false
    // 显示对方的指针；关掉时只有本机自己的指针
    property bool remoteCursor: true
    readonly property var shortcuts: video.shortcuts

    readonly property string status: session ? session.status : ""
    readonly property bool busy: status === "connecting" || status === "waiting" || status === "reconnecting"
                                 || status === "restarting" || status === "signingOut" || status === "waking"
    // 让对方电脑重启、关机、睡眠、注销以后连接断了：画面区域说它怎么了（叫醒它时也是）
    readonly property bool afterPower: status === "restarting" || status === "shutDown" || status === "asleep"
                                       || status === "signingOut" || status === "waking"
    // 睡眠、关机了的我的设备：从本机叫得醒时有"唤醒"按钮
    readonly property bool canWake: (status === "asleep" || status === "shutDown") && session !== null
                                    && session.wakeProblem.length === 0
    readonly property bool showVideo: status === "connected" || status === "reconnecting"
    // 控制对方：对方允许、自己没选"仅查看"
    readonly property bool controlling: session !== null && session.controlGranted && !viewOnly

    // 全屏（只有画面）：退出的组合键、手势（RemoteView.fullScreen）
    property bool fullScreen: false
    // 手机：手指怎么操作对方（RemoteView.touchMode）、放大的倍数、对方是不是 Mac（按键栏上写 ⌘ 还是 Win）
    property string touchMode
    readonly property real zoom: video.zoom
    readonly property bool remoteMac: video.remoteMac
    // 画面上方的状态条（重连中、电源操作的进展）离顶上多远、现在有没有：手机上让开浮在上面的按钮
    property real statusTop: 12
    readonly property bool statusShown: topBar.visible

    signal closeRequested()
    // 手机：按住不放起了作用（RemoteView）
    signal pressedAndHeld()
    signal exitFullScreenRequested()

    // 会话窗口"发送快捷键"菜单
    function sendShortcut(id) {
        video.sendShortcut(id)
    }

    // 手机：打的字、按键栏上的键、回到整个画面
    function sendText(text) {
        video.sendText(text)
    }
    function pressKey(key, modifiers) {
        video.pressKey(key, modifiers)
    }
    function resetZoom() {
        video.resetZoom()
    }

    // 键盘的按键给对方（连上了、又看到画面了、关掉了打字和面板）
    function focusInput() {
        if (showVideo && visible)
            video.forceActiveFocus()
    }

    color: Theme.canvas

    function submitPin() {
        if (pin.text.trim().length === 0) {
            pin.forceActiveFocus()
            return
        }
        root.session.connectWithPin(pin.text)
        pin.clear()
    }

    onStatusChanged: {
        if (status === "needPin")
            Qt.callLater(() => pin.forceActiveFocus())
        else if (status === "connected")
            Qt.callLater(focusInput)
    }
    // 切回这个标签、从主界面回到会话
    onVisibleChanged: {
        if (visible)
            Qt.callLater(focusInput)
    }
    Component.onCompleted: if (status === "needPin") Qt.callLater(() => pin.forceActiveFocus())

    // 远程画面：适应窗口时留黑边，原始大小时一个画面像素对一个屏幕像素
    RemoteView {
        id: video
        anchors.fill: parent
        visible: root.showVideo
        session: root.session
        nativeSize: root.nativeSize
        remoteCursor: root.remoteCursor
        inputEnabled: root.controlling && root.showVideo
        touchMode: root.touchMode
        fullScreen: root.fullScreen
        onPressedAndHeld: root.pressedAndHeld()
        onExitFullScreenRequested: root.exitFullScreenRequested()
    }

    // 连上了、第一帧还没到
    Text {
        anchors.centerIn: parent
        visible: root.showVideo && !video.hasFrame
        text: qsTr("等待远程画面…")
        font.pixelSize: 13
        color: Theme.textMuted
    }

    // 画面上方的状态：重连中；让对方电脑重启、关机、睡眠、注销的请求走到哪一步了
    Rectangle {
        id: topBar

        readonly property var request: root.session !== null ? root.session.powerRequest : ({})
        readonly property string phase: root.status === "reconnecting" ? "" : request.phase ?? ""
        readonly property string actionName: {
            switch (request.action) {
            case "restart": return qsTr("重启")
            case "shutDown": return qsTr("关机")
            case "sleep": return qsTr("睡眠")
            case "signOut": return qsTr("注销")
            }
            return ""
        }

        anchors.horizontalCenter: parent.horizontalCenter
        y: root.statusTop
        visible: root.showVideo && (root.status === "reconnecting" || phase.length > 0)
        width: Math.min(topLabel.implicitWidth + 24, root.width - 48)
        height: 28
        radius: 14
        color: phase === "failed" ? "#e6c4281c" : "#cc1d1d1f"

        Text {
            id: topLabel
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 24)
            elide: Text.ElideRight
            font.pixelSize: 12
            color: "white"
            text: {
                if (root.status === "reconnecting")
                    return qsTr("连接中断，正在重新连接…")
                switch (topBar.phase) {
                case "asking": return qsTr("正在请对方电脑%1…").arg(topBar.actionName)
                case "accepted": return qsTr("对方电脑马上%1").arg(topBar.actionName)
                case "failed": return qsTr("没能%1：%2").arg(topBar.actionName).arg(topBar.request.message)
                }
                return ""
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 320)
        visible: !root.showVideo
        spacing: 10

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34

            Icon {
                anchors.centerIn: parent
                visible: !root.busy
                name: root.status === "failed" ? "x" : root.status === "needPin" ? "key"
                    : root.afterPower ? "power" : "monitor"
                size: 30
                color: root.status === "failed" ? Theme.danger : Theme.textMuted
            }
            Icon {
                anchors.centerIn: parent
                visible: root.busy
                name: "refresh"
                size: 28
                color: Theme.statusColor(root.status)

                RotationAnimation on rotation {
                    running: root.busy
                    from: 0
                    to: 360
                    duration: 1200
                    loops: Animation.Infinite
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
            text: {
                switch (root.status) {
                case "needPin": return qsTr("输入 %1 的临时密码").arg(root.name)
                case "connecting": return qsTr("正在连接 %1…").arg(root.name)
                case "waiting": return qsTr("等待 %1 允许连接…").arg(root.name)
                case "reconnecting": return qsTr("与 %1 的连接已中断，正在重新连接…").arg(root.name)
                case "restarting": return qsTr("%1 正在重启…").arg(root.name)
                case "shutDown": return qsTr("%1 已关机").arg(root.name)
                case "asleep": return qsTr("%1 已睡眠").arg(root.name)
                case "signingOut": return qsTr("%1 的用户已注销，正在连到登录界面…").arg(root.name)
                case "waking": return qsTr("正在叫醒 %1…").arg(root.name)
                case "failed": return qsTr("无法连接 %1").arg(root.name)
                default: return qsTr("已连接 %1，等待远程画面…").arg(root.name)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: {
                switch (root.status) {
                case "failed":
                case "needPin": return root.session.reason
                case "restarting": return qsTr("开机后会自动重新连上，最多等 5 分钟")
                // 叫得怎么样了（从本机发的，还是哪台帮忙发的）
                case "waking": return (root.session.reason.length > 0 ? root.session.reason + "\n" : "")
                                      + qsTr("醒了就自动连上，最多等 3 分钟")
                case "asleep": return root.canWake ? "" : qsTr("有人唤醒它以后，点“重新连接”")
                }
                return ""
            }
            font.pixelSize: 12
            color: root.status === "needPin" ? Theme.danger : Theme.textMuted
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: root.status === "needPin"
            spacing: 8

            AppTextField {
                id: pin
                Layout.fillWidth: true
                // 画面区域是灰色的，输入框用白底才看得出来
                fill: Theme.surface
                focusFill: Theme.surface
                placeholderText: qsTr("临时密码")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhSensitiveData
                maximumLength: 32
                onAccepted: root.submitPin()
            }
            AppButton {
                primary: true
                text: qsTr("连接")
                onClicked: root.submitPin()
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            visible: root.status === "failed" || root.status === "reconnecting" || root.status === "waiting"
                     || root.status === "connecting" || root.afterPower
            spacing: 8

            AppButton {
                visible: root.canWake
                primary: true
                iconName: "power"
                text: qsTr("唤醒")
                onClicked: root.session.wake()
            }
            AppButton {
                visible: root.status === "failed" || root.status === "asleep"
                primary: !root.canWake
                iconName: "refresh"
                text: root.status === "asleep" ? qsTr("重新连接") : qsTr("重试")
                onClicked: root.session.retry()
            }
            AppButton {
                text: root.status === "failed" || root.status === "shutDown" || root.status === "asleep" ? qsTr("关闭")
                    : root.status === "restarting" || root.status === "waking" ? qsTr("不等了")
                    : qsTr("取消")
                onClicked: root.closeRequested()
            }
        }
    }

    // 只能看：自己选了"仅查看"，或者对方只允许查看
    Rectangle {
        x: 12
        y: 12
        visible: root.showVideo && (root.viewOnly || !root.session.controlGranted)
        width: viewOnlyLabel.implicitWidth + 18
        height: 24
        radius: 12
        color: "#cc1d1d1f"

        Text {
            id: viewOnlyLabel
            anchors.centerIn: parent
            text: root.session !== null && root.session.controlGranted ? qsTr("仅查看") : qsTr("对方只允许查看")
            font.pixelSize: 12
            color: "white"
        }
    }
}
