import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 工具栏里对方的显示器：一块一个按钮（按对方桌面上从左到右编号），点一下看那一块，正在看的那块按下去。
// 对方只有一块时不显示。对方的画面是连着它的所有人共用的，别人换了这里也跟着变；
// 能控制对方才能换（只能看时换了会打断正在控制的人），只能看时按钮是灰的
RowLayout {
    id: root

    required property ViewerSession session
    readonly property RemoteDisplays displays: session ? session.displays : null
    readonly property bool canChoose: session !== null && session.controlGranted

    visible: displays !== null && displays.items.length > 1
    spacing: 2

    // 点开看每一块的缩略图（DisplayThumbnails），从里面挑
    IconButton {
        id: thumbnailsButton
        compact: !Theme.unifiedTitleBar
        iconName: "monitor"
        text: qsTr("显示器的缩略图")
        active: thumbnails.opened
        onClicked: thumbnails.show(thumbnailsButton)
    }

    DisplayThumbnails {
        id: thumbnails
        parent: Overlay.overlay
        displays: root.displays
        canChoose: root.canChoose
    }

    Repeater {
        model: root.displays ? root.displays.items : []
        delegate: AbstractButton {
            id: button

            required property int index
            required property var modelData
            readonly property bool current: index === root.displays.currentIndex

            // 给读屏软件；画出来的是编号
            text: modelData.name.length > 0 ? modelData.name : qsTr("显示器 %1").arg(index + 1)
            implicitWidth: Theme.titleButtonSize
            implicitHeight: Theme.titleButtonSize
            hoverEnabled: true
            enabled: root.canChoose
            onClicked: root.displays.show(index)

            background: Rectangle {
                radius: Theme.unifiedTitleBar ? Theme.radius : Theme.radius - 2
                color: button.current ? Theme.selected : button.hovered ? Theme.hover : "transparent"
            }
            contentItem: Text {
                text: button.index + 1
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 13
                font.weight: button.current ? Font.DemiBold : Font.Normal
                color: button.current ? Theme.text : button.enabled ? Theme.textSecondary : Theme.textMuted
            }
        }
    }
}
