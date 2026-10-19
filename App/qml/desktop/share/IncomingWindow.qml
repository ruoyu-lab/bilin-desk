import QtQuick
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts

// 被控时的提示，两部分：
// - 顶部刘海：有人连着本机时在鼠标所在那块屏幕的顶部正中，鼠标换到别的屏幕它跟过去。鼠标移上去
//   长成卡片：谁正连着本机（对话、发文件、控制权、断开）、最近的文件传输；移开后缩回刘海。
//   对方发来文件时自己长成卡片，问收不收，决定了再缩回去。
//   macOS 上放在菜单栏那一层、贴住屏幕顶端：有摄像头刘海的屏幕上和它合成一体，从摄像头刘海两边伸出来，
//   长成卡片时摄像头那一段嵌在卡片顶上；没有摄像头刘海的屏幕上是自己画的，盖住菜单栏正中、和菜单栏一样高
// - 连接请求卡片：有人请求连接时出现在鼠标所在屏幕的正中（允许控制 / 仅查看 / 拒绝），
//   可以拖到任何地方，记住位置
// 谁请求连接、谁连着本机、对话、互传的文件、剪贴板同步都是真的（IncomingConnections）
//
// 鼠标在哪、在不在刘海上按鼠标的屏幕坐标判断（CursorPosition），不用窗口的悬停事件：macOS 上程序不在前台时
// 收不到，卡片窗口盖到刘海上的那一下进出的先后也不一定
//
// 刘海是两个大小固定的窗口，动画期间窗口不改变大小（改大小会卡，Windows 上收起结束时会闪一下）：
// - 刘海窗口：有人连着时一直在
// - 卡片窗口：叠在刘海上面，面板在里面从刘海的样子长成卡片、再缩回去；缩回到和刘海一模一样时
//   藏起来，露出底下的刘海
// 根是不可见的对象，窗口挂在属性上：窗口写在 Item 里会成为附属窗口，Item 不在任何窗口里时永远不显示
QtObject {
    id: root

    readonly property var requests: IncomingConnections.requests
    readonly property var viewers: IncomingConnections.viewers
    // 和连着本机的人互传的文件：只看现在连着的人的（重连回来的人编号变了，他的传输跟着变）
    readonly property IncomingFiles files: IncomingConnections.files
    readonly property var fileOffers: files.offers.filter(o => viewers.some(v => v.id === o.viewer))
    readonly property var recentTransfers: files.transfers.filter(t => viewers.some(v => v.id === t.viewer)).slice(-2)
    readonly property bool connected: viewers.length > 0

    property bool hovering: false
    // 对方发来了文件、正在选发什么时也展开着
    readonly property bool expanded: hovering || fileOffers.length > 0 || sendMenu.opened
    // 0：刘海，1：卡片
    property real progress: expanded ? 1 : 0
    Behavior on progress { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    // 阴影留出的边距（贴着屏幕顶部的那一侧不留）、内边距
    readonly property int margin: 24
    readonly property int padding: 14
    readonly property int cardWidth: 340
    readonly property int cardHeight: card.implicitHeight + 2 * padding + cardTop

    // macOS：刘海和卡片放在菜单栏那一层，贴住屏幕顶端
    readonly property bool overMenuBar: Qt.platform.os === "osx"
    readonly property point screenCenter: Qt.point(screenLeft + screenWidth / 2, screenTop + screenHeight / 2)
    // 屏幕上的摄像头刘海；没有就是自己画的刘海
    readonly property rect cameraNotch: screenWidth > 0 ? WindowChrome.screenNotch(screenCenter) : Qt.rect(0, 0, 0, 0)
    readonly property bool merged: cameraNotch.width > 0
    readonly property real menuBarHeight: screenWidth > 0 ? WindowChrome.menuBarHeight(screenCenter) : 0
    // 对方的图标（最多三个）和未读消息排成一行的宽度
    readonly property int iconsWidth: Math.min(viewers.length, 3) * 20 - 6
                                      + (IncomingConnections.chat.unread > 0 ? 20 : 0)
    // 合体时摄像头刘海两边各伸出的一截：左边红点，右边对方的图标
    readonly property int ear: Math.max(28, iconsWidth + 24)
    readonly property int notchWidth: merged ? Math.round(cameraNotch.width) + 2 * ear : Math.max(96, iconsWidth + 51)
    readonly property int notchHeight: merged ? Math.round(cameraNotch.height)
                                     : menuBarHeight > 0 ? Math.round(menuBarHeight) : 30
    // 盖在菜单栏上时纯黑：合体时和摄像头刘海一样，看不出接缝。深色、浅色下都是黑的（Theme.notch）
    readonly property color notchColor: overMenuBar ? "black" : Theme.notch
    // 卡片内容从刘海那一条下面开始：合体时那一条压着摄像头，放不了东西
    readonly property int cardTop: merged ? notchHeight : 0
    readonly property real notchCenter: merged ? cameraNotch.x + cameraNotch.width / 2 : screenLeft + screenWidth / 2
    // 两个窗口的宽度。窗口的位置按这里算，不读窗口自己的 width：窗口挪动时系统会把大小一起报回来，
    // 读了就成了绑定循环
    readonly property int notchWindowWidth: notchWidth + 2 * margin
    readonly property int cardWindowWidth: cardWidth + 2 * margin
    // 卡片窗口里面板当前的大小
    readonly property real panelWidth: notchWidth + (cardWidth - notchWidth) * progress
    readonly property real panelHeight: notchHeight + (cardHeight - notchHeight) * progress

    readonly property CursorPosition cursor: CursorPosition {
        active: true
    }

    // point（Qt 的全局坐标）所在的屏幕，不在任何屏幕上时为 null
    function screenAt(point) {
        const screens = Qt.application.screens
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i]
            if (point.x >= s.virtualX && point.x < s.virtualX + s.width
                    && point.y >= s.virtualY && point.y < s.virtualY + s.height)
                return s
        }
        return null
    }

    function clamp(value, low, high) {
        return Math.max(low, Math.min(high, value))
    }

    function viewerName(id) {
        const viewer = viewers.find(v => v.id === id)
        return viewer ? viewer.name : ""
    }

    // 刘海所在的屏幕：跟着鼠标换，展开着时不换（缩回刘海以后再跟过去）
    readonly property var cursorScreen: screenAt(cursor.position)
    property var shownScreen: Qt.application.screens[0]
    function follow() {
        if (cursorScreen && progress === 0 && shownScreen !== cursorScreen)
            shownScreen = cursorScreen
    }
    Component.onCompleted: follow()
    onCursorScreenChanged: follow()
    onProgressChanged: follow()
    // 换了屏幕：窗口挪过去以后重新贴回那块屏幕的顶端
    onShownScreenChanged: Qt.callLater(() => {
        placeOverMenuBar(notchWindow)
        placeOverMenuBar(cardWindow)
    })

    // 屏幕（逻辑像素）：刘海所在的那块
    readonly property int screenLeft: shownScreen ? shownScreen.virtualX : 0
    readonly property int screenTop: shownScreen ? shownScreen.virtualY : 0
    readonly property int screenWidth: shownScreen ? shownScreen.width : 0
    readonly property int screenHeight: shownScreen ? shownScreen.height : 0

    // 鼠标在刘海上，或在长出来的卡片上（按面板当前的大小，缩回去时移开就不再算）。
    // 上沿按窗口实际的位置：系统可能把窗口往下挪过
    readonly property bool pointed: {
        const p = cursor.position
        const width = progress > 0 ? panelWidth : notchWidth
        const height = progress > 0 ? panelHeight : notchHeight
        const left = notchCenter - width / 2
        const top = notchWindow.y
        return connected && p.x >= left && p.x < left + width && p.y >= top && p.y < top + height
    }
    onPointedChanged: {
        if (pointed) {
            collapse.stop()
            hovering = true
        } else {
            collapse.restart()
        }
    }

    // macOS：刘海和卡片放到菜单栏那一层；窗口每次显示出来都要重新贴回屏幕顶端
    function placeOverMenuBar(window) {
        if (overMenuBar && window.visible)
            WindowChrome.placeOverMenuBar(window)
    }

    // 连接请求卡片：放回上次拖到的地方（那里还有屏幕的话，挪进那块屏幕里），否则放在鼠标所在屏幕的正中
    function placeRequest(window) {
        const saved = AppSettings.requestX >= 0
                    ? screenAt(Qt.point(AppSettings.requestX + window.width / 2, AppSettings.requestY + margin)) : null
        if (saved) {
            window.x = clamp(AppSettings.requestX, saved.virtualX, saved.virtualX + saved.width - window.width)
            window.y = clamp(AppSettings.requestY, saved.virtualY, saved.virtualY + saved.height - window.height)
            return
        }
        const s = cursorScreen || shownScreen
        window.x = s.virtualX + Math.round((s.width - window.width) / 2)
        window.y = s.virtualY + Math.round((s.height - window.height) / 2)
    }

    // 有人连着本机时要退出（托盘的"退出"、macOS 的 Cmd+Q）：先问一句。full：后台服务也退出
    function confirmQuit(full) {
        quitWindow.full = full
        const s = cursorScreen || shownScreen
        quitWindow.x = s.virtualX + Math.round((s.width - quitWindow.width) / 2)
        quitWindow.y = s.virtualY + Math.round((s.height - quitWindow.height) / 2)
        quitWindow.show()
        quitWindow.raise()
        quitWindow.requestActivate()
    }

    // 最后一个人走了：对话窗口关掉（对话记录还在），要问的退出也不用问了
    onConnectedChanged: {
        if (connected)
            return
        chat.close()
        quitWindow.close()
    }

    // 独立的顶层窗口，不依附于提示：在任务栏里有自己的图标
    readonly property ShareChatWindow chatWindow: ShareChatWindow {
        id: chat
        transientParent: null
    }

    // 鼠标离开后稍等再收回，免得掠过时来回闪
    readonly property Timer collapseTimer: Timer {
        id: collapse
        interval: 350
        onTriggered: root.hovering = false
    }

    readonly property FileDialog filePicker: FileDialog {
        property int viewerId
        title: qsTr("选择要发给对方的文件")
        fileMode: FileDialog.OpenFiles
        onAccepted: root.files.send(viewerId, selectedFiles)
    }

    readonly property FolderDialog folderPicker: FolderDialog {
        property int viewerId
        title: qsTr("选择要发给对方的文件夹")
        onAccepted: root.files.send(viewerId, [selectedFolder])
    }

    // 面板的底：圆角白底和阴影。阴影按圆角矩形直接画（RectangularShadow），大小变化时不用重算，动画顺
    component Surface: Item {
        id: surface
        property alias color: background.color
        property alias shadowOpacity: shadow.opacity
        readonly property real radius: Theme.cardRadius
        default property alias content: body.data

        RectangularShadow {
            id: shadow
            anchors.fill: background
            radius: background.radius
            blur: 18
            offset: Qt.vector2d(0, 4)
            color: Qt.rgba(0, 0, 0, 0.14)
        }
        Rectangle {
            id: background
            anchors.fill: parent
            radius: surface.radius
            color: Theme.surface
        }
        Item {
            id: body
            anchors.fill: parent
            clip: true
        }
    }

    // 刘海里的东西：红点，对方的系统图标和未读消息。
    // topInset：面板伸出屏幕顶部的那一段（藏起上沿的圆角），内容在刘海那一条里竖直居中
    component NotchContent: Item {
        id: notchContent
        property real topInset: 0
        readonly property real middle: topInset + root.notchHeight / 2

        // 自己画的刘海：横着排在中间
        Row {
            visible: !root.merged
            x: (parent.width - width) / 2
            y: notchContent.middle - height / 2
            spacing: 8
            RecordingDot { anchors.verticalCenter: parent.verticalCenter }
            ViewerIcons { anchors.verticalCenter: parent.verticalCenter }
        }
        // 和摄像头刘海合体：摄像头那一段放不了东西，红点在左边，对方的图标在右边
        RecordingDot {
            visible: root.merged
            x: (parent.width - root.cameraNotch.width - root.ear - width) / 2
            y: notchContent.middle - height / 2
        }
        ViewerIcons {
            visible: root.merged
            x: (parent.width + root.cameraNotch.width + root.ear - width) / 2
            y: notchContent.middle - height / 2
        }
    }

    component RecordingDot: Rectangle {
        width: 7
        height: 7
        radius: 3.5
        color: Theme.danger
    }

    component ViewerIcons: Row {
        spacing: 6

        Repeater {
            model: root.viewers.slice(0, 3)
            delegate: Icon {
                required property var modelData
                name: Theme.osStyle(modelData.platform).icon
                size: 14
                color: "white"
            }
        }
        Icon {
            visible: IncomingConnections.chat.unread > 0
            name: "message"
            size: 14
            color: "white"
        }
    }

    // 按住拖动窗口，跟着鼠标走；动了几个像素才算拖，免得吃掉点击
    component DragArea: MouseArea {
        id: drag
        required property Window target
        property point grab
        property bool moving: false
        signal finished()

        anchors.fill: parent
        cursorShape: moving ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: mouse => {
            const p = drag.mapToGlobal(mouse.x, mouse.y)
            grab = Qt.point(p.x - target.x, p.y - target.y)
            moving = false
        }
        onPositionChanged: mouse => {
            const p = drag.mapToGlobal(mouse.x, mouse.y)
            if (!moving && Math.abs(p.x - target.x - grab.x) + Math.abs(p.y - target.y - grab.y) < 6)
                return
            moving = true
            target.x = p.x - grab.x
            target.y = p.y - grab.y
        }
        onReleased: {
            if (!moving)
                return
            moving = false
            drag.finished()
        }
    }

    component Person: RowLayout {
        id: person
        required property var info
        property string subtitle
        spacing: 10

        DeviceIcon {
            os: person.info.platform
            size: 34
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: person.info.name
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                text: person.subtitle
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Theme.textSecondary
            }
        }
    }

    // ---- 刘海：有人连着时一直在，卡片缩回来后露出来 ----
    readonly property Window notchPanel: Window {
        id: notchWindow

        transientParent: null
        // 不用 Qt.Tool：macOS 上工具窗口在程序不在前台时会隐藏。不抢键盘焦点：出现时不打断本机正在打字的人
        flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
               | Qt.WindowDoesNotAcceptFocus
        color: "transparent"
        visible: root.connected
        width: root.notchWindowWidth
        height: root.notchHeight + root.margin
        x: Math.round(root.notchCenter - root.notchWindowWidth / 2)
        y: root.screenTop

        onVisibleChanged: root.placeOverMenuBar(notchWindow)

        Surface {
            id: notchSurface
            // 上沿伸出窗口外，不露圆角
            x: root.margin
            y: -radius
            width: root.notchWidth
            height: root.notchHeight + radius
            color: root.notchColor
            // 盖在菜单栏上时没有阴影：合体时和摄像头刘海连成一片
            shadowOpacity: root.overMenuBar ? 0 : 1

            NotchContent {
                anchors.fill: parent
                topInset: notchSurface.radius
            }
        }
    }

    // ---- 卡片：叠在刘海上面，面板从刘海的样子长成卡片 ----
    readonly property Window cardPanel: Window {
        id: cardWindow

        transientParent: null
        flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
               | Qt.WindowDoesNotAcceptFocus
        color: "transparent"
        // 缩回到和刘海一样时才藏起来
        visible: root.connected && (root.expanded || root.progress > 0)
        width: root.cardWindowWidth
        height: root.cardHeight + root.margin
        x: Math.round(root.notchCenter - root.cardWindowWidth / 2)
        y: root.screenTop

        // 每次出现都在刘海上面
        onVisibleChanged: {
            if (!visible)
                return
            root.placeOverMenuBar(cardWindow)
            raise()
        }

        Surface {
            id: cardSurface
            // 从刘海往下、往两边长；上沿伸出窗口外不露圆角，颜色从刘海的深色过渡到卡片的浅色
            x: (root.cardWindowWidth - root.panelWidth) / 2
            y: -radius
            width: root.panelWidth
            height: root.panelHeight + radius
            color: Qt.tint(root.notchColor, Qt.alpha(Theme.surface, root.progress))
            shadowOpacity: root.overMenuBar ? root.progress : 1

            NotchContent {
                anchors.fill: parent
                topInset: cardSurface.radius
                opacity: 1 - root.progress
                visible: opacity > 0
            }

            ColumnLayout {
                id: card
                opacity: root.progress
                visible: opacity > 0
                x: root.padding
                y: cardSurface.radius + root.cardTop + root.padding
                width: root.cardWidth - 2 * root.padding
                spacing: 14

                Repeater {
                    model: root.viewers
                    delegate: RowLayout {
                        id: viewer
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4

                        Person {
                            Layout.fillWidth: true
                            info: viewer.modelData
                            subtitle: viewer.modelData.control ? qsTr("正在控制本机") : qsTr("正在查看本机")
                        }
                        IconButton {
                            readonly property Conversation conversation:
                                IncomingConnections.chat.conversation(viewer.modelData.id)
                            iconName: "message"
                            text: qsTr("对话")
                            active: conversation !== null && conversation.unread > 0
                            onClicked: root.chatWindow.present(viewer.modelData.id)
                        }
                        IconButton {
                            id: upload
                            iconName: "upload"
                            text: qsTr("发送文件")
                            popup: sendMenu
                            onOpenRequested: {
                                sendMenu.viewerId = viewer.modelData.id
                                sendMenu.popup(upload, 0, upload.height + 4)
                            }
                        }
                        IconButton {
                            // 没有辅助功能授权时只能收回，不能给
                            enabled: viewer.modelData.control || IncomingConnections.canControl
                            iconName: viewer.modelData.control ? "pointer" : "eye"
                            text: viewer.modelData.control ? qsTr("改为仅查看") : qsTr("允许控制")
                            onClicked: IncomingConnections.setControl(viewer.modelData.id, !viewer.modelData.control)
                        }
                        AppButton {
                            Layout.leftMargin: 4
                            danger: true
                            text: qsTr("断开")
                            onClicked: IncomingConnections.disconnectViewer(viewer.modelData.id)
                        }
                    }
                }

                // 所有连着的人一起：关掉以后对方那边会看到"对方关了剪贴板同步"
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("同步剪贴板")
                        font.pixelSize: 13
                        color: Theme.textSecondary
                    }
                    AppSwitch {
                        checked: IncomingConnections.clipboard
                        onToggled: IncomingConnections.clipboard = checked
                    }
                }

                // 对方发来的文件，等本机的人决定
                Repeater {
                    model: root.fileOffers
                    delegate: FileOfferCard {
                        required property var modelData
                        Layout.fillWidth: true
                        files: root.files
                        offer: modelData
                        from: root.viewerName(modelData.viewer)
                    }
                }

                Repeater {
                    // 按位置取：传输中进度一秒变几次，不跟着重建
                    model: root.recentTransfers.length
                    delegate: TransferRow {
                        required property int index
                        Layout.fillWidth: true
                        files: root.files
                        transfer: root.recentTransfers[index]
                    }
                }
            }

            // 发给谁：点了哪个人的"发送文件"
            AppMenu {
                id: sendMenu
                property int viewerId

                AppMenuItem {
                    text: qsTr("发送文件…")
                    onTriggered: {
                        root.filePicker.viewerId = sendMenu.viewerId
                        root.filePicker.open()
                    }
                }
                AppMenuItem {
                    text: qsTr("发送文件夹…")
                    onTriggered: {
                        root.folderPicker.viewerId = sendMenu.viewerId
                        root.folderPicker.open()
                    }
                }
            }
        }
    }

    // ---- 连接请求：自由摆放的卡片，拖到哪里停在哪里 ----
    readonly property Window requestPanel: Window {
        id: requestWindow

        transientParent: null
        flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
               | Qt.WindowDoesNotAcceptFocus
        color: "transparent"
        visible: root.requests.length > 0
        width: root.cardWidth + 2 * root.margin
        height: requestList.implicitHeight + 2 * root.padding + 2 * root.margin

        onVisibleChanged: {
            if (!visible)
                return
            root.placeRequest(requestWindow)
            raise()
            appear.restart()
        }

        NumberAnimation {
            id: appear
            target: requestSurface
            property: "opacity"
            from: 0
            to: 1
            duration: 160
            easing.type: Easing.OutCubic
        }

        Surface {
            id: requestSurface
            x: root.margin
            y: root.margin
            width: root.cardWidth
            height: requestList.implicitHeight + 2 * root.padding

            DragArea {
                target: requestWindow
                onFinished: {
                    AppSettings.requestX = Math.round(requestWindow.x)
                    AppSettings.requestY = Math.round(requestWindow.y)
                }
            }

            ColumnLayout {
                id: requestList
                x: root.padding
                y: root.padding
                width: root.cardWidth - 2 * root.padding
                spacing: 14

                Repeater {
                    model: root.requests
                    delegate: ColumnLayout {
                        id: request
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12

                        Person {
                            Layout.fillWidth: true
                            info: request.modelData
                            // 网页版来的没有设备 ID
                            subtitle: request.modelData.deviceId.length > 0 ? qsTr("请求连接本机 · %1").arg(request.modelData.deviceId)
                                                                           : qsTr("请求连接本机")
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: !IncomingConnections.canControl
                            text: qsTr("还没授予\"辅助功能\"权限，现在只能让对方看")
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            AppButton {
                                text: qsTr("拒绝")
                                onClicked: IncomingConnections.refuse(request.modelData.id)
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                text: qsTr("仅查看")
                                onClicked: IncomingConnections.admit(request.modelData.id, false)
                            }
                            AppButton {
                                primary: true
                                enabled: IncomingConnections.canControl
                                text: qsTr("允许控制")
                                onClicked: IncomingConnections.admit(request.modelData.id, true)
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- 有人连着本机时要退出：问一句，确认了断开他们再退出 ----
    readonly property Window quitPanel: Window {
        id: quitWindow

        property bool full: false

        transientParent: null
        flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
        color: "transparent"
        width: root.cardWidth + 2 * root.margin
        height: quitContent.implicitHeight + 2 * root.padding + 2 * root.margin

        Shortcut {
            sequence: StandardKey.Cancel
            onActivated: quitWindow.close()
        }

        Surface {
            x: root.margin
            y: root.margin
            width: root.cardWidth
            height: quitContent.implicitHeight + 2 * root.padding

            ColumnLayout {
                id: quitContent
                x: root.padding
                y: root.padding
                width: root.cardWidth - 2 * root.padding
                spacing: 12

                Text {
                    text: qsTr("有人正连着本机")
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("退出以后，%1 会被断开。").arg(root.viewers.map(v => v.name).join("、"))
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("取消")
                        onClicked: quitWindow.close()
                    }
                    AppButton {
                        danger: true
                        text: qsTr("断开并退出")
                        onClicked: {
                            quitWindow.close()
                            if (quitWindow.full)
                                ThisDevice.quitService()
                            AppLifecycle.quit()
                        }
                    }
                }
            }
        }
    }
}
