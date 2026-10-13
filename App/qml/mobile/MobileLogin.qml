import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 手机的登录页：第一次打开时（没登录、没选过"以后再说"），设置里没登录时点账号卡片也到这里。
// 上面是 Logo 和一句话，下面是和电脑一样的登录 / 注册 / 找回密码表单（LoginForm）
Item {
    id: root

    // 点了"以后再说"
    signal skipRequested()

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, column.implicitHeight + column.y + root.SafeArea.margins.bottom + 24)
        clip: true

        ColumnLayout {
            id: column
            x: Math.round((flick.width - width) / 2)
            y: root.SafeArea.margins.top + 40
            width: Math.min(flick.width - 48, 420)
            height: Math.max(implicitHeight, flick.height - y - root.SafeArea.margins.bottom - 24)
            spacing: 6

            Logo {
                Layout.alignment: Qt.AlignHCenter
                size: 72
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                text: qsTr("比邻")
                font.pixelSize: 26
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                Layout.bottomMargin: 22
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("登录同一个账号，手机就能直接连你的电脑")
                font.pixelSize: 15
                color: Theme.textSecondary
            }

            LoginForm {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 420
                onSkipRequested: root.skipRequested()
            }
        }
    }
}
