import QtQuick
import QtQuick.Controls

// Windows 的窗口按钮：最小化、最大化 / 还原、关闭。和侧栏开关同一种小号圆角按钮，竖直居中；
// 由程序画、登记给 QWindowKit（系统据此处理悬停、贴靠布局）。macOS 用系统的红绿灯，不显示
Row {
    id: root

    property bool maximizable: true
    // 左边有工具时，前面加一条细线把它们和窗口按钮分开
    property bool separated: false
    readonly property Window window: Window.window
    readonly property bool maximized: window !== null && window.visibility === Window.Maximized
    // 按钮的边长；它离这一行上下边的距离，放在窗口右上角时也用作离右边的距离
    readonly property int buttonSize: Theme.titleButtonSize
    readonly property int inset: Math.round((height - buttonSize) / 2)

    visible: !Theme.unifiedTitleBar
    spacing: 4

    component CaptionButton: AbstractButton {
        id: button
        property string iconName
        property bool closing: false
        property int iconSize: Theme.titleIconSize

        anchors.verticalCenter: parent.verticalCenter
        width: root.buttonSize
        height: root.buttonSize
        hoverEnabled: true

        background: Rectangle {
            radius: Theme.radius
            color: button.closing && (button.hovered || button.down) ? (button.down ? "#c42b1c" : "#e81123")
                 : button.down ? Theme.selected : button.hovered ? Theme.hover : "transparent"
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: button.iconName
                size: button.iconSize
                color: button.closing && button.hovered ? "white" : Theme.textSecondary
            }
        }
    }

    Item {
        visible: root.separated
        width: 9
        height: root.height

        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: 16
            color: Theme.border
        }
    }

    CaptionButton {
        id: minimize
        iconName: "window-minimize"
        onClicked: root.window.showMinimized()
        Component.onCompleted: if (root.visible) WindowChrome.setWindowButton(minimize, "minimize")
    }
    CaptionButton {
        id: maximize
        visible: root.maximizable
        iconName: root.maximized ? "window-restore" : "window-maximize"
        iconSize: Theme.titleIconSize + 2 // 方框图形本身偏小，放大一点和另外两个看起来一样大
        onClicked: root.maximized ? root.window.showNormal() : root.window.showMaximized()
        Component.onCompleted: if (root.visible && root.maximizable) WindowChrome.setWindowButton(maximize, "maximize")
    }
    CaptionButton {
        id: close
        closing: true
        iconName: "x"
        onClicked: root.window.close()
        Component.onCompleted: if (root.visible) WindowChrome.setWindowButton(close, "close")
    }
}
