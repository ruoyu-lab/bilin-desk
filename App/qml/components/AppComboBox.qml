import QtQuick
import QtQuick.Controls

// 下拉框：浅灰底，右边一个向下的箭头；弹出来的列表和菜单（AppMenu）一个样子，选中的那项前面打勾。
// model 是 [{label, value}] 这样的列表（textRole: "label"）；选了哪个看 activated(index)
ComboBox {
    id: root

    textRole: "label"
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 30
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.5

    background: Rectangle {
        radius: Theme.radius
        color: root.hovered || root.popup.visible ? Theme.fillHover : Theme.fill
    }

    contentItem: Text {
        text: root.displayText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        font: root.font
        color: Theme.text
    }

    indicator: Icon {
        x: root.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 13
        color: Theme.textSecondary
    }

    delegate: ItemDelegate {
        id: item
        required property var modelData
        required property int index

        width: ListView.view.width
        implicitHeight: 30
        leftPadding: 32
        rightPadding: 12
        highlighted: root.highlightedIndex === index

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                radius: 6
                color: item.highlighted ? Theme.hover : "transparent"
            }
        }
        contentItem: Text {
            text: item.modelData[root.textRole]
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 13
            color: Theme.text
        }
        Icon {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: item.index === root.currentIndex
            name: "check"
            size: 13
            color: Theme.text
        }
    }

    popup: Popup {
        y: root.height + 4
        width: Math.max(root.width, contentItem.implicitWidth)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)
        topPadding: 5
        bottomPadding: 5
        leftPadding: 0
        rightPadding: 0

        background: PopupSurface { radius: 10 }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
    }
}
