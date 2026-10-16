import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 胶囊式会话标签：等宽排列，空间不够时收窄到下限，再不够就横向滚动。
// 标签可拖动排序，右键（触屏上长按）打开菜单；标签前的状态点反映连接状态。
Rectangle {
    id: root

    required property SessionStage stage

    readonly property int maxTabWidth: 160
    readonly property int minTabWidth: 96
    readonly property int tabSpacing: 2
    readonly property int arrowWidth: 20

    readonly property int count: stage.sessions.count
    readonly property real innerWidth: width - 4
    readonly property bool overflowing: count * minTabWidth + (count - 1) * tabSpacing > innerWidth
    readonly property real stripWidth: overflowing ? innerWidth - arrowWidth * 2 : innerWidth
    readonly property real tabWidth: overflowing ? minTabWidth
                                     : Math.min(maxTabWidth, (innerWidth - (count - 1) * tabSpacing) / Math.max(1, count))
    readonly property real slot: tabWidth + tabSpacing

    function scrollTo(x) {
        scrollAnimation.to = Math.max(0, Math.min(strip.contentWidth - strip.width, x))
        scrollAnimation.restart()
    }

    function reveal(index) {
        const left = index * slot
        const right = left + tabWidth
        if (left < strip.contentX)
            scrollTo(left)
        else if (right > strip.contentX + strip.width)
            scrollTo(right - strip.width)
    }

    implicitWidth: count * maxTabWidth + Math.max(0, count - 1) * tabSpacing + 4
    implicitHeight: 28
    radius: height / 2
    color: Theme.fill

    onOverflowingChanged: Qt.callLater(() => reveal(stage.currentIndex))

    Connections {
        target: root.stage
        function onCurrentIndexChanged() { root.reveal(root.stage.currentIndex) }
    }

    IconButton {
        x: 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.overflowing
        enabled: strip.contentX > 0
        implicitWidth: root.arrowWidth
        implicitHeight: 24
        iconSize: 12
        iconName: "chevron-right"
        rotation: 180
        onClicked: root.scrollTo(strip.contentX - root.slot * 2)
    }
    IconButton {
        x: root.width - width - 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.overflowing
        enabled: strip.contentX < strip.contentWidth - strip.width
        implicitWidth: root.arrowWidth
        implicitHeight: 24
        iconSize: 12
        iconName: "chevron-right"
        onClicked: root.scrollTo(strip.contentX + root.slot * 2)
    }

    Flickable {
        id: strip

        x: 2 + (root.overflowing ? root.arrowWidth : 0)
        y: 2
        width: root.stripWidth
        height: 24
        contentWidth: Math.max(width, root.count * root.slot - root.tabSpacing)
        contentHeight: height
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        NumberAnimation {
            id: scrollAnimation
            target: strip
            property: "contentX"
            duration: 180
            easing.type: Easing.OutCubic
        }

        WheelHandler {
            enabled: root.overflowing
            onWheel: event => {
                const delta = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
                scrollAnimation.stop()
                strip.contentX = Math.max(0, Math.min(strip.contentWidth - strip.width, strip.contentX - delta))
            }
        }

        Repeater {
            model: root.stage.sessions
            delegate: AbstractButton {
                id: tab
                required property int index
                required property string name
                required property string key
                readonly property ViewerSession viewer: ViewerSessions.get(key)
                readonly property string status: viewer ? viewer.status : ""
                readonly property bool current: index === root.stage.currentIndex
                // 组件创建完成后才启用位置动画，避免新标签从左边滑入
                property bool settled: false

                width: root.tabWidth
                height: 24
                z: drag.active ? 1 : 0
                hoverEnabled: true
                onPressed: root.stage.select(index)
                onXChanged: {
                    if (!drag.active)
                        return
                    const target = Math.max(0, Math.min(root.count - 1, Math.round(x / root.slot)))
                    if (target !== index)
                        root.stage.moveSession(index, target)
                }
                Component.onCompleted: settled = true

                Binding on x {
                    when: !drag.active
                    value: tab.index * root.slot
                    restoreMode: Binding.RestoreNone
                }
                Behavior on x {
                    enabled: tab.settled && !drag.active
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }

                DragHandler {
                    id: drag
                    target: tab
                    yAxis.enabled: false
                    xAxis.minimum: 0
                    xAxis.maximum: strip.contentWidth - tab.width
                }
                // 右键在点的地方出菜单；手指按住在标签下面出（不让手指挡住）。右键只认鼠标：不然手指点一下也算
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onTapped: (point, button) => root.stage.showTabMenu(tab.index, tab, point.position.x, point.position.y)
                }
                TapHandler {
                    enabled: Theme.touch
                    acceptedDevices: PointerDevice.TouchScreen
                    onLongPressed: root.stage.showTabMenu(tab.index, tab, 0, tab.height + 6)
                }

                // 用透明度过渡而不是颜色过渡："transparent" 是透明的黑色，插值会经过发黑的中间色
                background: Item {
                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: Theme.hover
                        opacity: tab.hovered && !tab.current ? 1 : 0

                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: Theme.surface
                        opacity: tab.current ? 1 : 0

                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                }

                contentItem: RowLayout {
                    id: content
                    spacing: 6

                    StatusDot {
                        Layout.leftMargin: 10
                        status: tab.status
                    }
                    Text {
                        Layout.fillWidth: true
                        text: tab.name
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: tab.current ? Theme.text : Theme.textSecondary
                    }
                    // 不在看的会话里对方说了话
                    Icon {
                        visible: !tab.current && tab.viewer !== null && tab.viewer.chat.conversation.unread > 0
                        name: "message"
                        size: 12
                        color: Theme.text
                    }
                    IconButton {
                        Layout.rightMargin: 4
                        implicitWidth: 18
                        implicitHeight: 18
                        iconSize: 10
                        // 触屏上没有悬停：正在看的那个一直有
                        opacity: tab.hovered || (Theme.touch && tab.current) ? 1 : 0
                        iconName: "x"
                        text: qsTr("断开")
                        onClicked: root.stage.closeSession(tab.index)
                    }
                }
            }
        }
    }
}
