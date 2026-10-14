import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property int index
    required property string key
    required property string name
    required property string address
    required property bool viewOnly
    property bool panelOpen: true

    readonly property ViewerSession session: ViewerSessions.get(key)
    // 缩放：适应窗口，或者原始大小（一个画面像素对一个屏幕像素）
    property bool nativeSize: false
    // 会话窗口"发送快捷键"菜单：对方系统上的组合键 [{id, label, keys}]
    readonly property var shortcuts: canvas.shortcuts

    signal closeRequested()

    // shortcuts 里 id 那一项
    function sendShortcut(id) {
        canvas.sendShortcut(id)
    }

    // 让对方电脑 "restart" | "shutDown" | "sleep" | "signOut"（右上角电源菜单里确认过了）
    function requestPower(action) {
        session.requestPower(action)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        RemoteCanvas {
            id: canvas
            Layout.fillWidth: true
            Layout.fillHeight: true
            session: root.session
            name: root.name
            viewOnly: root.viewOnly
            nativeSize: root.nativeSize
            remoteCursor: AppSettings.remoteCursor
            onCloseRequested: root.closeRequested()
        }

        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: root.panelOpen ? 300 : 0
            clip: true

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            SidePanel {
                id: panel
                width: 300
                height: parent.height
                view: root
                fileOffers: root.session ? root.session.files.offers.length : 0
            }
        }
    }

    // 对方发来文件、窗口不在前面时让系统提醒一下（macOS 的 Dock 图标跳动，Windows 的任务栏按钮闪）
    Connections {
        target: root.session ? root.session.files : null
        function onOffersChanged() {
            if (root.session.files.offers.length > 0)
                root.Window.window.alert(0)
        }
    }

    // 对方发来文件、文件页没在眼前时：在画面下方问收不收（一次问一份）
    Repeater {
        model: root.session !== null && !(root.panelOpen && panel.tab === 1) ? root.session.files.offers.slice(0, 1) : []
        delegate: FileOfferCard {
            required property var modelData
            x: canvas.x + Math.round((canvas.width - width) / 2)
            y: canvas.height - height - 20
            width: Math.min(canvas.width - 48, 380)
            elevated: true
            files: root.session.files
            offer: modelData
            from: root.name
        }
    }
}
