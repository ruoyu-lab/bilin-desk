import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// 安装、卸载的界面：一页，正中一个按钮；装的时候按钮换成进度条，装完换成"打开比邻"。
// 卸载是同一个样子。真的在 Setup 里（Windows：安装模式启动的程序；macOS：设置里的卸载）。
// 安装包怎么来的见 docs/install-and-first-run.md
ApplicationWindow {
    id: win

    readonly property bool installing: Setup.mode === "install"
    readonly property string status: Setup.status

    // 固定大小（同登录窗口：限制用常量，不绑到 width / height）
    width: 560
    height: 360
    minimumWidth: 560
    maximumWidth: 560
    minimumHeight: 360
    maximumHeight: 360
    title: installing ? qsTr("安装比邻") : qsTr("卸载比邻")
    color: Theme.bg
    flags: Theme.windowFlags
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    Component.onCompleted: WindowChrome.applyTitleBar(win, "compact")
    // 装到一半不能关
    onClosing: close => {
        if (status === "busy")
            close.accepted = false
    }

    component Link: Text {
        signal activated
        font.pixelSize: 12
        color: Theme.textSecondary
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.activated()
        }
    }

    // 整个窗口按住都能拖
    WindowDragArea { anchors.fill: parent }

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
        height: Theme.titleBarHeight
        maximizable: false
    }

    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -10
        width: 400
        spacing: 8

        // 程序的 Logo；完成后换成对勾
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64

            Logo {
                anchors.fill: parent
                visible: win.status !== "done"
                size: 64
            }
            Rectangle {
                anchors.fill: parent
                visible: win.status === "done"
                radius: 32
                color: Qt.rgba(0.2, 0.78, 0.35, 0.14)
                Icon {
                    anchors.centerIn: parent
                    name: "check"
                    size: 30
                    color: Theme.success
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            text: win.status === "done" ? (win.installing ? qsTr("装好了") : qsTr("已卸载"))
                : win.status === "failed" ? (win.installing ? qsTr("没装成") : qsTr("没卸载完"))
                : win.installing ? qsTr("比邻") : qsTr("卸载比邻")
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: win.status === "done" ? (win.installing ? qsTr("你的其他设备现在就能连这台电脑")
                                                           : qsTr("这台电脑上的比邻已经删干净了"))
                : win.status === "failed" ? Setup.error
                : win.installing ? qsTr("随时连接你的每一台设备") : qsTr("卸载后，你的其他设备就连不上这台电脑了")
            font.pixelSize: 13
            color: win.status === "failed" ? Theme.danger : Theme.textSecondary
        }

        // 按钮和进度条在同一个位置
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 16
            implicitWidth: 240
            implicitHeight: 40

            AppButton {
                anchors.centerIn: parent
                width: 200
                implicitHeight: 40
                visible: win.status !== "busy"
                dark: true
                text: win.status === "done" ? (win.installing ? qsTr("打开比邻") : qsTr("完成"))
                    : win.status === "failed" ? qsTr("再试一次")
                    : win.installing ? qsTr("立即安装") : qsTr("卸载")
                onClicked: {
                    if (win.status === "ready" || win.status === "failed") {
                        Setup.start()
                    } else if (win.installing) {
                        Setup.openInstalled()
                        win.close()
                    } else {
                        win.close()
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 240
                height: 4
                visible: win.status === "busy"
                radius: 2
                color: Theme.fill

                Rectangle {
                    width: parent.width * Setup.progress
                    height: parent.height
                    radius: 2
                    color: Theme.text
                }
            }
        }

        // 按钮下面一行小字：安装位置 / 进度 / 完成、取消
        Item {
            Layout.fillWidth: true
            Layout.topMargin: 4
            implicitHeight: 18

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: win.status === "ready" && win.installing
                spacing: 8
                Text {
                    text: qsTr("安装到 %1").arg(Setup.installPath)
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
                Link {
                    text: qsTr("更改")
                    color: Theme.accent
                    onActivated: folderPicker.open()
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: win.status === "busy"
                text: (win.installing ? qsTr("正在安装… %1%") : qsTr("正在卸载… %1%")).arg(Math.round(Setup.progress * 100))
                font.pixelSize: 12
                color: Theme.textSecondary
            }
            Link {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: (win.status === "done" && win.installing) || ((win.status === "ready" || win.status === "failed")
                                                                       && !win.installing)
                text: win.status === "done" ? qsTr("完成") : qsTr("取消")
                onActivated: win.close()
            }
        }

        CheckOption {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            visible: win.installing && win.status === "ready" && Qt.platform.os === "windows"
            text: qsTr("在桌面上创建快捷方式")
            checked: Setup.desktopShortcut
            onToggled: Setup.desktopShortcut = checked
        }
    }

    // 桌面快捷方式的勾选框（Windows 安装时）：点一下换勾没勾（按钮自带的 checked、toggled）
    component CheckOption: AbstractButton {
        id: option
        checkable: true
        implicitHeight: 20
        implicitWidth: optionRow.implicitWidth
        contentItem: Row {
            id: optionRow
            spacing: 7
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 15
                height: 15
                radius: 4
                color: option.checked ? Theme.inverse : "transparent"
                border.width: option.checked ? 0 : 1
                border.color: Theme.borderStrong
                Icon {
                    anchors.centerIn: parent
                    visible: option.checked
                    name: "check"
                    size: 10
                    color: Theme.inverseText
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: option.text
                font.pixelSize: 12
                color: Theme.textSecondary
            }
        }
    }

    FolderDialog {
        id: folderPicker
        title: qsTr("安装到")
        onAccepted: Setup.setFolder(selectedFolder)
    }
}
