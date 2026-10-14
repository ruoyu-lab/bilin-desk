import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    required property BrowseState browse
    // 右边要让出的宽度（Windows 的窗口按钮）
    property real rightInset: 0
    signal addRequested

    // 顶部按钮弹出的浮窗和菜单，在按钮下面：
    // macOS 一律靠窗口右边（离右边 popupMargin）；Windows 左边对齐按钮，放不下时往左挪，离右边至少 popupMargin
    readonly property int popupMargin: 10

    function placeBelow(popup, button) {
        popup.parent = Overlay.overlay
        const anchor = button.mapToItem(Overlay.overlay, 0, button.height)
        const rightmost = Overlay.overlay.width - popup.width - popupMargin
        popup.x = Theme.unifiedTitleBar ? rightmost : Math.min(anchor.x, rightmost)
        popup.y = anchor.y + 8
    }

    implicitHeight: Theme.headerHeight

    WindowDragArea { anchors.fill: parent }

    RowLayout {
        id: tools
        anchors.right: parent.right
        anchors.rightMargin: root.rightInset > 0 ? root.rightInset : 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.unifiedTitleBar ? 8 : 4
        visible: root.browse.view !== "settings"

        readonly property bool listing: root.browse.view !== "home"
        Component.onCompleted: WindowChrome.setInteractive(tools)

        ToolGroup {
            visible: !parent.listing
            IconButton {
                compact: true
                iconName: "plus"
                text: qsTr("添加设备")
                onClicked: root.addRequested()
            }
        }

        ToolGroup {
            visible: !parent.listing
            IconButton {
                id: bellButton
                compact: true
                iconName: "bell"
                text: qsTr("通知")
                onClicked: {
                    root.placeBelow(notifications, bellButton)
                    notifications.open()
                }

                // 有没看过的连接记录
                Rectangle {
                    visible: IncomingConnections.history.unread > 0
                    x: parent.width - width - 5
                    y: 5
                    width: 7
                    height: 7
                    radius: 3.5
                    color: Theme.danger
                }
            }
        }

        ToolGroup {
            visible: parent.listing
            IconButton {
                id: searchButton
                compact: true
                iconName: "search"
                text: qsTr("搜索")
                active: root.browse.search.length > 0 || searchPopup.opened
                activeFill: Theme.unifiedTitleBar ? Theme.surface : Theme.selected
                // 按下按钮时浮窗已因"点了外面"关掉，这次点击不再打开（再点一次按钮即关闭）
                onClicked: {
                    if (Date.now() - searchPopup.closedAt < 300)
                        return
                    root.placeBelow(searchPopup, searchButton)
                    searchPopup.open()
                }
            }
            IconButton {
                id: sortButton
                compact: true
                iconName: "sort"
                text: qsTr("排序")
                onClicked: {
                    root.placeBelow(sortMenu, sortButton)
                    sortMenu.open()
                }
            }
            IconButton {
                id: filterButton
                compact: true
                iconName: "filter"
                text: qsTr("筛选")
                active: root.browse.filtering
                activeFill: Theme.unifiedTitleBar ? Theme.surface : Theme.selected
                onClicked: {
                    root.placeBelow(filterMenu, filterButton)
                    filterMenu.open()
                }
            }
        }

        ToolGroup {
            visible: parent.listing
            IconButton {
                compact: true
                iconName: "panel-right"
                text: root.browse.detailOpen ? qsTr("收起详情") : qsTr("展开详情")
                onClicked: root.browse.detailOpen = !root.browse.detailOpen
            }
        }
    }

    // 搜索：点了按钮才在它下面弹出一个小浮窗（像浏览器的查找），输入即筛选；
    // 关掉浮窗不清空，按钮保持高亮表示正在筛选
    Popup {
        id: searchPopup
        property real closedAt: 0

        width: 260
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface {}
        onOpened: searchField.focusInput()
        onClosed: closedAt = Date.now()

        contentItem: SearchBox {
            id: searchField
            placeholder: qsTr("搜索名称、设备 ID 或地址")
            clearOnEscape: false
            text: root.browse.search
            onTextChanged: root.browse.search = text
        }
    }

    AppMenu {
        id: sortMenu
        AppMenuItem { markable: true; marked: root.browse.sort === "name"; text: qsTr("按名称"); onTriggered: root.browse.sort = "name" }
        AppMenuItem { markable: true; marked: root.browse.sort === "recent"; text: qsTr("按最近连接"); onTriggered: root.browse.sort = "recent" }
        AppMenuItem { markable: true; marked: root.browse.sort === "online"; text: qsTr("在线优先"); onTriggered: root.browse.sort = "online" }
    }

    AppMenu {
        id: filterMenu
        AppMenuItem { markable: true; marked: root.browse.onlyOnline; text: qsTr("仅显示在线"); onTriggered: root.browse.onlyOnline = !root.browse.onlyOnline }
        AppMenuSeparator {}
        AppMenuItem { markable: true; marked: root.browse.os === ""; text: qsTr("全部系统"); onTriggered: root.browse.os = "" }
        AppMenuItem { markable: true; marked: root.browse.os === "Windows"; text: "Windows"; onTriggered: root.browse.os = "Windows" }
        AppMenuItem { markable: true; marked: root.browse.os === "macOS"; text: "macOS"; onTriggered: root.browse.os = "macOS" }
        AppMenuItem { markable: true; marked: root.browse.os === "Linux"; text: "Linux"; onTriggered: root.browse.os = "Linux" }
    }

    // 通知：本机的连接记录（IncomingConnections.history），还有程序自己更新了，打开就算看过了
    Popup {
        id: notifications

        readonly property var entries: IncomingConnections.history.entries

        // 一条记录说的话
        function describe(entry) {
            const who = entry.name.length > 0 ? entry.name : entry.deviceId
            switch (entry.kind) {
            case "connection": {
                const how = entry.control ? qsTr("控制") : qsTr("查看")
                if (!entry.endedAt)
                    return qsTr("%1 正在%2本机").arg(who).arg(how)
                return qsTr("%1 %2了本机 %3").arg(who).arg(how).arg(duration(entry.endedAt - entry.at))
            }
            case "refused":
                return entry.reason === "refused" ? qsTr("拒绝了 %1 的连接请求").arg(who)
                     : entry.reason === "timeout" ? qsTr("%1 请求连接本机，一直没人允许").arg(who)
                     : qsTr("%1 请求连接本机，那时没开着本程序").arg(who)
            case "wrongPin":
                return qsTr("有人输错了临时密码")
            case "file": {
                const what = entry.count > 1 ? qsTr("%1 等 %2 项").arg(entry.file).arg(entry.count) : entry.file
                return entry.sent ? qsTr("发给 %1：%2").arg(who).arg(what) : qsTr("收到 %1 发来的 %2").arg(who).arg(what)
            }
            case "updated":
                return qsTr("程序更新到了 %1").arg(entry.version)
            case "power":
                switch (entry.action) {
                case "restart": return qsTr("%1 远程重启了本机").arg(who)
                case "shutDown": return qsTr("%1 远程关闭了本机").arg(who)
                case "sleep": return qsTr("%1 让本机睡眠了").arg(who)
                case "signOut": return qsTr("%1 远程注销了本机的用户").arg(who)
                }
                return ""
            }
            return ""
        }

        function duration(ms) {
            const minutes = Math.round(ms / 60000)
            if (minutes < 1)
                return qsTr("不到 1 分钟")
            if (minutes < 60)
                return qsTr("%1 分钟").arg(minutes)
            return qsTr("%1 小时 %2 分钟").arg(Math.floor(minutes / 60)).arg(minutes % 60)
        }

        // 今天的只写时间，昨天的写"昨天"，再早写日期
        function when(date) {
            const today = new Date()
            const day = new Date(date.getFullYear(), date.getMonth(), date.getDate())
            const days = Math.round((new Date(today.getFullYear(), today.getMonth(), today.getDate()) - day) / 86400000)
            const time = Qt.formatTime(date, "hh:mm")
            if (days === 0)
                return time
            if (days === 1)
                return qsTr("昨天 %1").arg(time)
            return Qt.formatDate(date, today.getFullYear() === date.getFullYear() ? "M月d日" : "yyyy年M月d日") + " " + time
        }

        width: 320
        padding: 16
        background: PopupSurface {}
        onOpened: IncomingConnections.history.markSeen()

        contentItem: ColumnLayout {
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("通知")
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.text
                }
                Text {
                    visible: notifications.entries.length > 0
                    text: qsTr("清空")
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: IncomingConnections.history.clear()
                    }
                }
            }
            Text {
                visible: notifications.entries.length === 0
                text: qsTr("谁连过本机、互传的文件、程序的更新会记在这里")
                font.pixelSize: 13
                color: Theme.textMuted
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 360)
                visible: notifications.entries.length > 0
                clip: true
                spacing: 10
                boundsBehavior: Flickable.StopAtBounds
                model: notifications.entries
                ScrollBar.vertical: ScrollBar {}
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: notifications.describe(modelData)
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        color: modelData.kind === "wrongPin" ? Theme.danger : Theme.text
                    }
                    Text {
                        Layout.fillWidth: true
                        text: notifications.when(modelData.at)
                              + (modelData.deviceId.length > 0 ? " · " + modelData.deviceId : "")
                        font.pixelSize: 11
                        color: Theme.textMuted
                    }
                }
            }
        }
    }
}
