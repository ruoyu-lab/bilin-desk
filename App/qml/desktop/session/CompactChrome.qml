import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 细标题栏：红绿灯、会话标签和工具共用一条窄栏。全屏时外观不变。
Item {
    id: root

    required property ApplicationWindow session

    Rectangle {
        id: bar
        width: parent.width
        height: root.session.compactBarHeight
        color: Theme.sidebar
        Component.onCompleted: WindowChrome.setTitleBar(bar)

        WindowDragArea { anchors.fill: parent; enabled: !root.session.fullScreen }

        RowLayout {
            anchors.fill: parent
            // 全屏时红绿灯平时不显示，鼠标顶到上边缘时系统会把它滑出来，位置留着避免盖住标签
            anchors.leftMargin: root.session.titleButtons.width > 0
                                ? root.session.titleButtons.x + root.session.titleButtons.width + 14 : 10
            anchors.rightMargin: windowButtons.visible ? windowButtons.inset : 8
            spacing: 8

            SessionTabs {
                id: tabs
                session: root.session
                Layout.maximumWidth: parent.width - tools.width - (windowButtons.visible ? windowButtons.width : 0) - 24
                Component.onCompleted: WindowChrome.setInteractive(tabs)
            }

            Item { Layout.fillWidth: true }

            SessionTools {
                id: tools
                session: root.session
                Component.onCompleted: WindowChrome.setInteractive(tools)
            }

            // Windows：全屏时不显示
            WindowButtons {
                id: windowButtons
                separated: true
                Layout.preferredHeight: bar.height
                visible: !Theme.unifiedTitleBar && !root.session.fullScreen
            }
        }
    }
}
