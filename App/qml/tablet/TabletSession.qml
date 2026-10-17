pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 平板上连着电脑：和电脑版的会话窗口一样，上面一条细栏（左边回到主界面、各台的标签，右边工具），下面是画面和
// 侧边面板（SessionStage）。几台一起连，标签切换；回到主界面时连接不断（TabletMain 的侧栏里能回来）。
// 平板另有：键盘（打字、按键栏，TouchKeyboard）、手指操作方式、放大时的倍数（点一下复原）
Item {
    id: root

    // 手指怎么操作对方：touchpad | direct（TabletMain 按设置定）
    property string fingerMode: "direct"
    readonly property SessionStage stage: stage
    readonly property int count: stage.sessions.count
    readonly property SessionView view: stage.currentView
    readonly property bool controlling: view !== null && view.session !== null && view.session.controlGranted
                                        && !view.viewOnly

    // 全屏：只有画面（上面那条、按键栏都藏起来），四个手指按住 3 秒出来；回到主界面时也出来
    property bool fullScreen: false
    onVisibleChanged: {
        if (!visible)
            fullScreen = false
    }

    // 回到主界面；最后一台断开了也是
    signal backRequested()

    function open(device, viewOnly) {
        stage.open(device, viewOnly)
    }

    // 连着的时候屏幕不自己熄灭（设置里能关）
    Binding {
        target: PhoneWindow
        property: "keepAwake"
        value: AppSettings.keepScreenOn && root.count > 0
    }
    // 看着会话、控制着的时候接着的键盘按 Win+R、Alt+Tab 也给对方，不给平板
    Binding {
        target: PhoneWindow
        property: "captureKeys"
        value: root.visible && root.controlling
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    Rectangle {
        id: bar
        visible: !root.fullScreen
        width: parent.width
        height: root.SafeArea.margins.top + Theme.headerHeight
        color: Theme.sidebar

        RowLayout {
            anchors.fill: parent
            anchors.topMargin: root.SafeArea.margins.top
            anchors.leftMargin: root.SafeArea.margins.left + 8
            anchors.rightMargin: root.SafeArea.margins.right + 8
            spacing: 8

            IconButton {
                id: back
                iconName: "chevron-right"
                rotation: 180
                text: qsTr("回到主界面")
                onClicked: root.backRequested()
            }

            SessionTabs {
                stage: stage
                Layout.maximumWidth: parent.width - back.width - tools.width - 40
            }

            Item { Layout.fillWidth: true }

            SessionTools {
                id: tools
                stage: stage
                fullScreenAvailable: true
                fullScreen: root.fullScreen
                onFullScreenToggled: {
                    keyboard.close()
                    root.fullScreen = true
                }
                leadingTools: Component {
                    RowLayout {
                        visible: root.controlling
                        spacing: 2

                        // 打字、按键栏；开着的时候点一下收起
                        IconButton {
                            id: keyboardButton
                            compact: true
                            iconName: "keyboard"
                            text: qsTr("键盘")
                            active: keyboard.active
                            popup: keyboard.menu
                            onOpenRequested: keyboard.active ? keyboard.close() : keyboard.showMenu(keyboardButton)
                        }
                        // 手指操作方式：触摸板、直接点，点一下换
                        AbstractButton {
                            id: mode
                            Layout.preferredHeight: back.height
                            leftPadding: 10
                            rightPadding: 10
                            onClicked: AppSettings.fingerMode = root.fingerMode === "touchpad" ? "direct" : "touchpad"
                            background: Rectangle {
                                radius: Theme.radius
                                color: mode.pressed ? Theme.hover : "transparent"
                            }
                            contentItem: Text {
                                text: root.fingerMode === "touchpad" ? qsTr("触摸板") : qsTr("直接点")
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 13
                                color: Theme.text
                            }
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignCenter
                            Layout.margins: 4
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 16
                            color: Theme.border
                        }
                    }
                }
            }
        }
    }

    SessionStage {
        id: stage
        anchors.top: root.fullScreen ? parent.top : bar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.SafeArea.margins.left
        anchors.rightMargin: root.SafeArea.margins.right
        // 让开按键栏、系统键盘
        anchors.bottomMargin: keyboard.active ? keyboard.occupied : root.SafeArea.margins.bottom
        touchMode: root.fingerMode
        fullScreen: root.fullScreen
        onExitFullScreenRequested: root.fullScreen = false
        onEmptied: root.backRequested()
        // 按住不放出了右键、开始拖：震一下
        onPressedAndHeld: PhoneWindow.pressFeedback()
    }

    // 放大了：倍数，点一下复原
    Rectangle {
        visible: root.view !== null && root.view.zoom > 1.01 && !root.fullScreen
        anchors.horizontalCenter: stage.horizontalCenter
        y: bar.height + 12
        width: zoomText.implicitWidth + 28
        height: 32
        radius: 16
        color: "#c71c1c1e"
        Text {
            id: zoomText
            anchors.centerIn: parent
            text: root.view !== null ? qsTr("%1%").arg(Math.round(root.view.zoom * 100)) : ""
            font.pixelSize: 13
            color: "white"
        }
        TapHandler {
            onTapped: root.view.resetZoom()
        }
    }

    TouchKeyboard {
        id: keyboard
        anchors.fill: parent
        target: root.view
    }
}
