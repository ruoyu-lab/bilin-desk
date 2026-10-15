import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// 显示器的缩略图：工具栏上显示器图标弹出来，一块一张（按对方桌面上从左到右），下面是编号和分辨率，
// 点哪块看哪块。打开时向对方要一次缩略图（截好一块来一块，还没到的先空着）。只能看时不能换，都是灰的。
// 不超出窗口：窗口放不下时每一张一起缩小
Popup {
    id: root

    property RemoteDisplays displays
    property bool canChoose

    // 弹出时按钮的左下角（在 parent 里）；靠右放不下时往左挪
    property real anchorX
    readonly property int count: displays ? displays.items.length : 0
    // 一张卡片多宽：最宽 216，窗口窄时平分窗口（左右各留 8）
    readonly property real cardWidth: count === 0 ? 216
        : Math.min(216, Math.floor((parent.width - 16 - leftPadding - rightPadding - (count - 1) * row.spacing) / count))

    function show(anchor) {
        displays.requestThumbnails()
        const point = anchor.mapToItem(parent, 0, anchor.height + 8)
        anchorX = Math.round(point.x)
        y = Math.round(point.y)
        open()
    }

    x: Math.max(8, Math.min(anchorX, parent.width - implicitWidth - 8))
    padding: 10
    background: PopupSurface { radius: 10 }

    contentItem: RowLayout {
        id: row
        spacing: 8

        Repeater {
            model: root.displays ? root.displays.items : []
            delegate: AbstractButton {
                id: card

                required property int index
                required property var modelData
                readonly property bool current: root.displays !== null && index === root.displays.currentIndex
                // 缩略图按对方显示器的比例，宽是卡片去掉左右的边距
                readonly property real pictureWidth: root.cardWidth - leftPadding - rightPadding
                readonly property real pictureHeight: Math.round(pictureWidth * modelData.height / Math.max(1, modelData.width))

                // 比例不同的几块顶端对齐
                Layout.alignment: Qt.AlignTop
                text: modelData.name.length > 0 ? modelData.name : qsTr("显示器 %1").arg(index + 1)
                enabled: root.canChoose
                hoverEnabled: true
                // 缩略图四周留 8；下面那行字在缩略图和卡片底边正中间（上下空得一样）
                leftPadding: 8
                rightPadding: 8
                topPadding: 8
                bottomPadding: 0
                implicitWidth: root.cardWidth
                implicitHeight: topPadding + pictureHeight + 34
                onClicked: {
                    root.displays.show(index)
                    root.close()
                }

                background: Rectangle {
                    radius: 8
                    color: card.current ? Theme.selected : card.hovered ? Theme.hover : "transparent"
                }
                contentItem: ColumnLayout {
                    spacing: 0
                    Rectangle {
                        Layout.preferredWidth: card.pictureWidth
                        Layout.preferredHeight: card.pictureHeight
                        radius: 4
                        color: Theme.canvas
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: card.modelData.thumbnail
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            opacity: card.enabled ? 1 : 0.5
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: (card.index + 1) + (card.modelData.main ? qsTr(" · 主显示器") : "")
                              + "　" + card.modelData.width + " × " + card.modelData.height
                        font.pixelSize: 12
                        font.weight: card.current ? Font.DemiBold : Font.Normal
                        color: card.current ? Theme.text : card.enabled ? Theme.textSecondary : Theme.textMuted
                    }
                }
            }
        }
    }
}
