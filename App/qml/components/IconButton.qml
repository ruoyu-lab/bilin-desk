import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: root

    // text（按钮自带的）只给读屏软件：画出来的只有图标
    property string iconName
    // Windows 上紧凑按钮和顶部其他按钮统一大小（Theme.titleButtonSize）
    property int iconSize: compact && Theme.unifiedTitleBar ? 15 : 16
    property bool active: false
    property bool filled: false
    property bool danger: false
    // 紧凑：放在工具组和搜索框里的小号按钮
    property bool compact: false
    property color activeFill: Theme.selected
    // 开关一个弹出的东西（菜单、弹出框）：点了发 openRequested，在那里打开它；开着时再点只是关掉（PopupToggle）
    property alias popup: toggle.popup

    signal openRequested()
    property color iconColor: filled ? Theme.accentText
                            : danger ? Theme.danger
                            : active ? Theme.text : Theme.textSecondary

    implicitWidth: compact && Theme.unifiedTitleBar ? 28 : Theme.titleButtonSize
    implicitHeight: compact && Theme.unifiedTitleBar ? 28 : Theme.titleButtonSize
    hoverEnabled: Theme.hoverEffects
    opacity: enabled ? 1 : 0.4
    onPressed: toggle.press()
    onClicked: {
        if (toggle.popup !== null && toggle.opening)
            root.openRequested()
    }

    PopupToggle {
        id: toggle
    }

    background: Rectangle {
        radius: root.compact ? Theme.radius - 2 : Theme.radius
        color: root.filled ? (root.down ? Theme.accentPressed : root.hovered ? Theme.accentHover : Theme.accent)
             : root.active ? root.activeFill
             : root.hovered || root.down ? Theme.hover : "transparent"
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: root.iconSize
            color: root.iconColor
        }
    }
}
