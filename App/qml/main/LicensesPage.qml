import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 开源声明：本程序和它用到的开源代码、各自的许可。点一项看许可全文
Item {
    id: root

    // 同设置页（SettingsScroll）
    property bool overTitleBar: false
    property real leftInset: 0

    signal backRequested

    // 正在看许可全文的那一项；null 时是列表
    property var current: null

    // files：许可全文，程序里带着（见 App/CMakeLists.txt 里的 licenses）
    readonly property var entries: [
        { name: qsTr("比邻"), purpose: qsTr("本程序"), license: "AGPL-3.0", files: ["AGPL-3.0.txt"] },
        { name: qsTr("远程控制 SDK"), purpose: qsTr("画面、声音和传输"), license: "MIT",
          files: ["RemoteControlSDK.txt", "RemoteControlSDK-third-party.txt"],
          note: qsTr("SDK 里用到的其他开源代码的许可附在后面") },
        { name: "Qt", purpose: qsTr("界面框架"), license: "LGPL-3.0", files: ["LGPL-3.0.txt", "GPL-3.0.txt"],
          note: qsTr("Qt 自带的其他开源组件见 <a href=\"%1\">Qt 的说明</a>")
                .arg("https://doc.qt.io/qt-6/licenses-used-in-qt.html") },
        { name: "QWindowKit", purpose: qsTr("Windows 的窗口外框"), license: "Apache-2.0", files: ["QWindowKit.txt"],
          windowsOnly: true },
        { name: "Lucide", purpose: qsTr("图标"), license: "ISC", files: ["Lucide.txt"] }
    ].filter(e => !e.windowsOnly || Qt.platform.os === "windows")

    function back() {
        if (current)
            current = null
        else
            root.backRequested()
    }

    onVisibleChanged: if (!visible) current = null

    SettingsScroll {
        id: scroll
        anchors.fill: parent
        title: root.current ? root.current.name : qsTr("开源声明")
        backTo: root.current ? qsTr("开源声明") : qsTr("关于比邻")
        overTitleBar: root.overTitleBar
        leftInset: root.leftInset
        onBackRequested: root.back()

        // ---- 列表 ----
        Text {
            Layout.fillWidth: true
            visible: !root.current
            text: qsTr("本程序用到了下面这些开源代码，感谢它们的作者。")
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        SettingsSection {
            visible: !root.current
            Repeater {
                model: root.entries
                delegate: SettingsRow {
                    required property var modelData
                    label: modelData.name
                    detail: modelData.purpose + " · " + modelData.license
                    navigates: true
                    onClicked: {
                        root.current = modelData
                        scroll.scrollToTop()
                    }
                }
            }
        }

        // ---- 许可全文 ----
        Text {
            Layout.fillWidth: true
            visible: root.current !== null
            text: root.current ? root.current.purpose + " · " + root.current.license : ""
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        Text {
            Layout.fillWidth: true
            visible: root.current !== null && root.current.note !== undefined
            text: root.current && root.current.note ? root.current.note : ""
            textFormat: Text.StyledText
            linkColor: Theme.accent
            wrapMode: Text.Wrap
            font.pixelSize: 13
            color: Theme.textSecondary
            onLinkActivated: link => Qt.openUrlExternally(link)
            HoverHandler {
                enabled: parent.hoveredLink.length > 0
                cursorShape: Qt.PointingHandCursor
            }
        }
        Rectangle {
            Layout.fillWidth: true
            visible: root.current !== null
            implicitHeight: license.implicitHeight + 36
            radius: Theme.cardRadius
            color: Theme.card

            TextEdit {
                id: license
                x: 18
                y: 18
                width: parent.width - 36
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                text: root.current ? root.current.files.map(f => Documents.text("licenses/" + f)).join("\n\n\n") : ""
                font.family: Qt.platform.os === "osx" ? "Menlo" : "Consolas"
                font.pixelSize: 12
                color: Theme.text
                selectionColor: Theme.textSelection
                selectedTextColor: Theme.text
            }
        }
    }
}
