import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 设置和它点进去的下一页的外框：上下能滚的一列（最宽 640，居中），最上面是标题；下一页的标题左边有返回。
// macOS 上页面盖在窗口的标题栏底下，顶上只留 30px 拖动窗口；左上角被红绿灯、侧栏开关压住的宽度，标题要让开
Item {
    id: root

    default property alias content: column.data
    property string title
    // 有返回：读屏软件念的、返回到哪一页的名字
    property string backTo
    property bool overTitleBar: false
    property real leftInset: 0

    signal backRequested()

    function scrollToTop() {
        flick.contentY = 0
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.y + column.height + 36
        boundsBehavior: Flickable.StopAtBounds
        // macOS：鼠标按住不拖内容，顶上那一条按住拖的是窗口（主窗口工具栏那一层）；滚动用触控板、滚轮
        acceptedButtons: root.overTitleBar ? Qt.NoButton : Qt.LeftButton
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            x: Math.round((flick.width - width) / 2)
            y: root.overTitleBar ? 30 : 4
            width: Math.min(flick.width - 48, 640)
            spacing: 20

            RowLayout {
                Layout.leftMargin: Math.max(0, root.leftInset - column.x)
                spacing: 6
                IconButton {
                    visible: root.backTo.length > 0
                    Layout.leftMargin: -8
                    iconName: "chevron-right"
                    rotation: 180
                    text: root.backTo
                    onClicked: root.backRequested()
                }
                Text {
                    Layout.fillWidth: true
                    text: root.title
                    elide: Text.ElideRight
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
            }
        }
    }
}
