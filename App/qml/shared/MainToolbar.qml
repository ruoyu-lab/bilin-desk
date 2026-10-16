import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    required property BrowseState browse
    // 右边要让出的宽度（Windows 的窗口按钮）
    property real rightInset: 0
    // 概览时右边多出来的工具（电脑版的通知）
    property Component homeTools
    // 右上角那一组（Windows：登记成标题栏里能点的）
    readonly property alias tools: tools
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

        ToolGroup {
            visible: !parent.listing
            IconButton {
                compact: true
                iconName: "plus"
                text: qsTr("添加设备")
                onClicked: root.addRequested()
            }
        }

        // 电脑版：通知（本机的连接记录，NotificationsButton）
        Loader {
            visible: !parent.listing && item !== null
            sourceComponent: root.homeTools
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
                popup: searchPopup
                onOpenRequested: {
                    root.placeBelow(searchPopup, searchButton)
                    searchPopup.open()
                }
            }
            IconButton {
                id: sortButton
                compact: true
                iconName: "sort"
                text: qsTr("排序")
                popup: sortMenu
                onOpenRequested: {
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
                popup: filterMenu
                onOpenRequested: {
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

        width: 260
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface {}
        onOpened: searchField.focusInput()

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
}
