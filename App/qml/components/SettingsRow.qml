import QtQuick
import QtQuick.Layouts

// 设置的一行（放在 SettingsSection 里）：左边一个小图标、名字（需要时下面一句说明），右边放开关、按钮这些；
// navigates 的一整行能点（悬停变色，右边一个箭头，箭头前面可以写 value），点了是 clicked。
// 和上面一行之间有一条细线（第一行没有）
Item {
    id: root

    default property alias trailing: slot.data
    property string iconName
    property string label
    property string detail
    property string value
    property bool navigates: false
    // 危险的操作（注销账号、卸载）：名字是红的
    property bool danger: false

    signal clicked()

    // 是不是这一组里第一个看得见的（Repeater 本身不算：它的高是 0）
    readonly property bool first: {
        const siblings = parent ? parent.children : []
        for (let i = 0; i < siblings.length; ++i) {
            if (siblings[i] === root)
                return true
            if (siblings[i].visible && siblings[i].height > 0)
                return false
        }
        return true
    }

    Layout.fillWidth: true
    implicitHeight: Theme.phone ? (detail.length > 0 ? 64 : 52) : (detail.length > 0 ? 56 : 44)

    Rectangle {
        anchors.fill: parent
        visible: root.navigates && hover.hovered
        radius: Theme.radius
        color: Theme.hover
    }

    // 分隔线从名字的左边开始
    Rectangle {
        visible: !root.first
        x: row.x + text.x
        width: parent.width - x - row.anchors.rightMargin
        height: 1
        color: Theme.border
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 12

        Rectangle {
            visible: root.iconName.length > 0
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: 6
            color: Theme.tile
            Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 14
                color: root.danger ? Theme.danger : Theme.text
            }
        }
        ColumnLayout {
            id: text
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: root.label
                elide: Text.ElideRight
                font.pixelSize: Theme.bodyFont
                color: root.danger ? Theme.danger : Theme.text
            }
            Text {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                elide: Text.ElideRight
                font.pixelSize: Theme.captionFont
                color: Theme.textSecondary
            }
        }
        Text {
            visible: root.value.length > 0
            Layout.maximumWidth: 220
            text: root.value
            elide: Text.ElideMiddle
            font.pixelSize: Theme.bodyFont
            color: Theme.textSecondary
        }
        Item {
            id: slot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
        Icon {
            visible: root.navigates
            name: "chevron-right"
            size: 14
            color: Theme.textMuted
        }
    }

    HoverHandler {
        id: hover
        enabled: root.navigates
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        enabled: root.navigates
        onTapped: root.clicked()
    }
}
