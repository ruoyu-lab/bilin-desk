import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 被控时的对话窗口：从刘海展开的卡片里点对话打开。普通窗口，不置顶。
// 每个连着本机的人一份对话（和他那台设备的对话记录，下次连上还在）；几个人连着时，标题下面一排名字切换，
// 有没看的消息的名字后面有红点。
// 顶部和程序的其他窗口统一：一行标题；macOS 是红绿灯，Windows 由 QWindowKit 管窗口外框
// （保留系统边框和贴靠布局），右上角是程序画的最小化和关闭。
ApplicationWindow {
    id: win

    // macOS：红绿灯所在的矩形，标题避开它，标题栏高度据此确定
    property rect titleButtons: Qt.rect(0, 0, 0, 0)
    readonly property int barHeight: !Theme.unifiedTitleBar ? Theme.titleBarHeight
                                   : titleButtons.height > 0 ? Math.round(titleButtons.y * 2 + titleButtons.height) : 34
    readonly property var viewers: IncomingConnections.viewers
    // 正在和谁说：连着本机的人的 id
    property int viewerId: -1
    readonly property var current: viewers.find(v => v.id === viewerId) ?? null
    readonly property Conversation conversation: current ? IncomingConnections.chat.conversation(current.id) : null

    // 打开和连着本机的人 id 的对话
    function present(id) {
        viewerId = id
        show()
        raise()
        requestActivate()
    }

    function markRead() {
        if (active && conversation)
            conversation.markRead()
    }

    width: 360
    height: 480
    minimumWidth: 300
    minimumHeight: 320
    title: current ? qsTr("对话 · %1").arg(current.name) : qsTr("对话")
    color: Theme.bg
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    // 位置、大小记下来，下次打开还在那里
    readonly property WindowMemory memory: WindowMemory {
        window: win
        key: "shareChat"
    }

    Component.onCompleted: {
        // 没记过时出现在屏幕右侧，只算这一次：写成绑定的话，改变窗口大小时位置会跟着宽高被拉回去
        x = Screen.virtualX + Screen.width - width - 80
        y = Screen.virtualY + Math.round((Screen.height - height) / 2)
        memory.restore()
        WindowChrome.applyTitleBar(win, "compact")
        Qt.callLater(() => win.titleButtons = WindowChrome.titleButtonsRect(win))
    }
    onActiveChanged: markRead()
    onConversationChanged: {
        // 换了人：上一个人那里没发出去的原因不再显示
        chatView.error = ""
        markRead()
    }
    // 正在和他说的人走了：换成还连着的第一个
    onViewersChanged: if (!current && viewers.length > 0) viewerId = viewers[0].id
    Connections {
        target: win.conversation
        function onUnreadChanged() { win.markRead() }
    }
    // 关掉只是藏起来（不销毁原生窗口，再打开时标题栏样式和大小不变）；程序退出时照常关
    onClosing: close => {
        if (!AppLifecycle.quitting) {
            close.accepted = false
            win.hide()
        }
    }

    // 标题栏：拖动窗口；Windows 上登记给 QWindowKit
    Rectangle {
        id: bar
        width: parent.width
        height: win.barHeight
        color: Theme.sidebar
        Component.onCompleted: WindowChrome.setTitleBar(bar)

        WindowDragArea { anchors.fill: parent }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.unifiedTitleBar && win.titleButtons.width > 0 ? win.titleButtons.x + win.titleButtons.width + 14 : 14
            width: parent.width - x - (windowButtons.visible ? windowButtons.width + 2 * windowButtons.inset : 14)
            text: win.title
            elide: Text.ElideRight
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.text
        }

        WindowButtons {
            id: windowButtons
            anchors.right: parent.right
            anchors.rightMargin: windowButtons.inset
            height: parent.height
            maximizable: false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: win.barHeight
        spacing: 0

        // 几个人连着本机：和谁说（放不下时左右拖）
        ListView {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.preferredHeight: 40
            visible: win.viewers.length > 1
            orientation: ListView.Horizontal
            spacing: 18
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: win.viewers

            delegate: UnderlineTab {
                id: person
                required property var modelData
                readonly property Conversation conversation: IncomingConnections.chat.conversation(modelData.id)
                text: modelData.name
                current: modelData.id === win.viewerId
                onClicked: win.viewerId = modelData.id

                // 有没看的消息
                Rectangle {
                    x: parent.width - 6
                    y: 9
                    width: 7
                    height: 7
                    radius: 3.5
                    color: Theme.danger
                    visible: !person.current && person.conversation !== null && person.conversation.unread > 0
                }
            }
        }

        ChatView {
            id: chatView
            Layout.fillWidth: true
            Layout.fillHeight: true
            messages: win.conversation ? win.conversation.messages : []
            notice: win.conversation ? win.conversation.storeError : ""
            placeholder: win.current ? qsTr("和 %1 说点什么").arg(win.current.name)
                                     : qsTr("和正在连着本机的人说点什么")
            send: text => IncomingConnections.chat.send(win.viewerId, text)
        }
    }
}
