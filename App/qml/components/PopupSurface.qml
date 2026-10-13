import QtQuick
import QtQuick.Effects

Rectangle {
    radius: Theme.cardRadius
    color: Theme.surface

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: "#000000"
        shadowOpacity: 0.16
        shadowBlur: 1.0
        shadowVerticalOffset: 6
    }
}
