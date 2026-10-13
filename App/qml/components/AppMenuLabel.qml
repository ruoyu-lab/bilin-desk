import QtQuick

// 菜单里一组项上面的小标题，或者最下面的一句说明：点不了
Text {
    leftPadding: 12
    rightPadding: 12
    topPadding: 4
    bottomPadding: 4
    // 没有要说的时不占地方
    height: text.length > 0 ? implicitHeight : 0
    visible: text.length > 0
    wrapMode: Text.Wrap
    font.pixelSize: 11
    color: Theme.textMuted
}
