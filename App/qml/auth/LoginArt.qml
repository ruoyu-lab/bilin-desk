import QtQuick
import QtQuick.Shapes

// 登录页左侧插画：中间是本机，周围是局域网里发现的设备
Item {
    id: root

    implicitWidth: 240
    implicitHeight: 150

    readonly property point center: Qt.point(width / 2, height / 2)
    readonly property var devices: [
        { x: 0.18, y: 0.27, online: true },
        { x: 0.84, y: 0.22, online: true },
        { x: 0.78, y: 0.80, online: false },
    ]

    Rectangle {
        anchors.centerIn: parent
        width: root.height * 0.9
        height: width
        radius: width / 2
        color: Theme.artOuter
    }
    Rectangle {
        anchors.centerIn: parent
        width: root.height * 0.55
        height: width
        radius: width / 2
        color: Theme.artInner
    }

    Repeater {
        model: root.devices.filter(d => d.online)
        delegate: Shape {
            required property var modelData
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.artLine
                strokeWidth: 1.2
                strokeStyle: ShapePath.DashLine
                dashPattern: [3, 3]
                fillColor: "transparent"
                startX: root.width * modelData.x
                startY: root.height * modelData.y
                PathLine { x: root.center.x; y: root.center.y }
            }
        }
    }

    Repeater {
        model: root.devices
        delegate: Item {
            required property var modelData
            x: root.width * modelData.x - 15
            y: root.height * modelData.y - 15
            width: 30
            height: 30

            Rectangle {
                anchors.fill: parent
                radius: 15
                color: Theme.surface
            }
            Icon {
                anchors.centerIn: parent
                name: "monitor"
                size: 14
                color: modelData.online ? Theme.textSecondary : Theme.textMuted
            }
            Rectangle {
                x: 21
                y: 1
                width: 8
                height: 8
                radius: 4
                color: modelData.online ? Theme.success : Theme.borderStrong
                border.width: 1.5
                border.color: Theme.introBg
            }
        }
    }

    Logo {
        anchors.centerIn: parent
        size: 52
    }
}
