import QtQuick

// 会话窗口右上角电源按钮的菜单：断开连接；连的是自己的设备、它允许时，还能让它重启、关机、睡眠、注销
// （选了以后先确认，PowerConfirm）。别人的电脑、没允许的、它的系统做不到的那几项都不显示
AppMenu {
    id: root

    // 菜单从哪个按钮弹出来，确认气泡也弹在它下面
    property Item anchor
    property ViewerSession session
    // 自己选了"仅查看"
    property bool viewOnly

    signal disconnectRequested()
    // "restart" | "shutDown" | "sleep" | "signOut"
    signal actionChosen(string action)

    function show(anchor) {
        root.anchor = anchor
        popup(anchor, anchor.width - width, anchor.height + 8)
    }

    readonly property bool connected: session !== null && session.status === "connected"
    // 对方允许、它的系统也做得到的（只有自己的设备会有）
    readonly property var offered: connected ? session.powerActions : []
    readonly property bool hasPower: offered.length > 0
    readonly property bool controlling: connected && session.controlGranted && !viewOnly
    // 上一个请求还没结束：等对方回答，或者对方马上就要做了
    readonly property bool pending: connected && (session.powerRequest.phase === "asking"
                                                  || session.powerRequest.phase === "accepted")
    readonly property string note: hasPower && !controlling ? qsTr("切换到控制以后才能重启、关机对方电脑") : ""

    AppMenuItem {
        text: qsTr("断开连接")
        onTriggered: root.disconnectRequested()
    }
    AppMenuSeparator {
        visible: root.hasPower
        height: visible ? implicitHeight : 0
    }
    AppMenuLabel {
        text: root.hasPower ? qsTr("对方电脑") : ""
    }
    Repeater {
        model: [
            { action: "restart", label: qsTr("重启") },
            { action: "shutDown", label: qsTr("关机") },
            { action: "sleep", label: qsTr("睡眠") },
            { action: "signOut", label: qsTr("注销") },
        ].filter(item => root.offered.includes(item.action))
        delegate: AppMenuItem {
            required property var modelData
            text: modelData.label
            enabled: root.controlling && !root.pending
            onTriggered: root.actionChosen(modelData.action)
        }
    }
    AppMenuLabel {
        text: root.note
    }
}
