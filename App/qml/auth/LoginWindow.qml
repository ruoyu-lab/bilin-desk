import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 登录窗口：左边介绍，右边登录 / 注册 / 找回密码的表单（LoginForm）；不登录可以只用局域网
ApplicationWindow {
    id: win

    // 固定大小。限制用常量而不是绑定到 width / height：Windows 加上边框后会改动 width，
    // 绑定会让最小值和最大值不一致
    width: 780
    height: 520
    minimumWidth: 780
    maximumWidth: 780
    minimumHeight: 520
    maximumHeight: 520
    title: form.title
    color: Theme.bg
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    // 按需创建、关掉就销毁（Main.qml）。关掉就是这次不登录了
    onClosing: {
        if (!AppLifecycle.quitting)
            Account.loginRequested = false
    }

    Component.onCompleted: WindowChrome.applyTitleBar(win, "unified")

    // ---- 左：介绍 ----
    Rectangle {
        id: intro
        width: Math.round(parent.width * 0.46)
        height: parent.height
        color: Theme.introBg

        WindowDragArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: Theme.toolbarHeight + 22
            anchors.leftMargin: 36
            anchors.rightMargin: 36
            anchors.bottomMargin: 80
            spacing: 8

            Text {
                text: qsTr("随时连接\n你的每一台设备")
                lineHeight: 1.15
                font.pixelSize: 26
                font.weight: Font.DemiBold
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("同一网络里直接发现，不在身边也能连上")
                wrapMode: Text.Wrap
                font.pixelSize: 13
                color: Theme.textSecondary
            }

            Item { Layout.fillHeight: true }

            LoginArt {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
            }
        }
    }

    // ---- 右：表单 ----
    Item {
        anchors.left: intro.right
        anchors.right: parent.right
        height: parent.height

        WindowDragArea {
            width: parent.width
            height: Theme.toolbarHeight
        }

        LoginForm {
            id: form
            anchors.fill: parent
            anchors.topMargin: Theme.toolbarHeight + 22
            anchors.leftMargin: 44
            anchors.rightMargin: 44
            anchors.bottomMargin: 30
            onSkipRequested: Account.loginRequested = false
        }
    }

    // Windows：顶部一行是标题栏，右上角是最小化和关闭（窗口大小固定，没有最大化）
    Item {
        id: titleBar
        width: parent.width
        height: Theme.headerHeight
        Component.onCompleted: WindowChrome.setTitleBar(titleBar)
    }

    WindowButtons {
        id: windowButtons
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: windowButtons.inset
        height: Theme.headerHeight
        maximizable: false
    }
}
