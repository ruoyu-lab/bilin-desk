import QtQuick

// 程序的 Logo（App/branding/logo.svg）：白色的圆角方块里一支指针，深色、浅色下都一样
Item {
    property int size: 64

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../branding/logo.svg")
        sourceSize: Qt.size(parent.size * 3, parent.size * 3)
        smooth: true
    }
}
