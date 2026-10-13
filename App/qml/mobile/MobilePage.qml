import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 手机上一个标签页的外框：最上面是大标题（右边可以放按钮），下面上下能滚（最宽 640，居中）。
// 让开状态栏，底下让开标签栏（bottomInset，MobileMain 给）
Item {
    id: root

    property string title
    default property alias content: column.data
    // 标题右边的按钮
    property alias actions: actionRow.data
    property real bottomInset: 0

    function scrollToTop() {
        flick.contentY = 0
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.y + column.height + 24 + root.bottomInset
        clip: true

        ColumnLayout {
            id: column
            x: Math.round((flick.width - width) / 2)
            y: root.SafeArea.margins.top + 8
            width: Math.min(flick.width - 32, 640)
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                Layout.bottomMargin: 2
                spacing: 4
                Text {
                    Layout.fillWidth: true
                    text: root.title
                    elide: Text.ElideRight
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                Row {
                    id: actionRow
                    spacing: 4
                }
            }
        }
    }
}
