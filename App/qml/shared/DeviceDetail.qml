import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

// 设备列表右边的详情：顶上是它桌面的背景（连上过、它发过来的；没有时是系统的图标），下面名字、连接，设备的信息、
// 备注，最下面复制地址、重命名、保存或移除。放不下时上下滚动
Item {
    id: root

    required property var device

    signal connectRequested(bool viewOnly)

    readonly property var style: Theme.osStyle(device.os)
    // 网络唤醒（Wake）：叫不醒时为什么；我的设备才有
    readonly property string wakeProblem: {
        Wake.revision
        return device.mine && device.deviceId ? Wake.problem(device.deviceId) : ""
    }
    readonly property bool wakeable: device.mine && device.presenceKnown && !device.online && wakeProblem.length === 0
    // 发过唤醒包的那一台：等它上线（device 随设备列表刷新会换成新的对象，按 key 认）
    property string wokenKey
    readonly property bool wakeSent: wokenKey === device.key

    // 没有值的不显示
    component InfoItem: ColumnLayout {
        property alias label: name.text
        property alias value: data.text
        visible: value.length > 0
        Layout.fillWidth: true
        spacing: 3
        Text {
            id: name
            font.pixelSize: 12
            color: Theme.textMuted
        }
        Text {
            id: data
            Layout.fillWidth: true
            elide: Text.ElideRight
            font.pixelSize: 13
            color: Theme.text
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        // 放得下时铺满（最下面那排按钮在底下），放不下时滚动
        contentHeight: Math.max(height, content.implicitHeight + 40)
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            x: 20
            y: 20
            width: flick.width - 40
            height: flick.contentHeight - 40
            spacing: 18

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, Math.round(width * 0.42))
                radius: 10
                color: root.style.tint

                readonly property bool hasWallpaper: root.device.wallpaper.toString().length > 0

                Image {
                    id: wallpaper
                    anchors.fill: parent
                    visible: false
                    source: root.device.wallpaper
                    // 原图读一次（对方发来的最长 1280 像素）：不跟着卡片的宽重新解码，改窗口大小时不闪
                    fillMode: Image.PreserveAspectCrop
                    cache: false
                    asynchronous: true
                }
                // 背景图切成和卡片一样的圆角
                Rectangle {
                    id: wallpaperMask
                    anchors.fill: parent
                    radius: 10
                    visible: false
                    layer.enabled: true
                }
                MultiEffect {
                    anchors.fill: parent
                    visible: parent.hasWallpaper && wallpaper.status === Image.Ready
                    source: wallpaper
                    maskEnabled: true
                    maskSource: wallpaperMask
                    opacity: root.device.presenceKnown && !root.device.online ? 0.6 : 1
                }

                Icon {
                    anchors.centerIn: parent
                    visible: !parent.hasWallpaper
                    name: root.style.icon
                    size: 44
                    color: root.style.ink
                    opacity: root.device.presenceKnown && !root.device.online ? 0.4 : 0.9
                }

                // 只有知道在不在线时（附近的）才显示
                Rectangle {
                    x: 12
                    y: 12
                    visible: root.device.presenceKnown
                    width: status.implicitWidth + 18
                    height: 24
                    radius: 12
                    color: Qt.alpha(Theme.surface, 0.7)

                    RowLayout {
                        id: status
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: root.device.online ? Theme.success : Theme.textMuted
                        }
                        Text {
                            text: root.device.online ? qsTr("在线") : qsTr("离线")
                            font.pixelSize: 12
                            color: Theme.text
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: root.device.name
                    elide: Text.ElideRight
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                    color: Theme.text
                }

                IconButton {
                    iconName: root.device.favorite ? "star-filled" : "star"
                    iconColor: root.device.favorite ? Theme.favorite : Theme.textSecondary
                    text: root.device.favorite ? qsTr("取消收藏") : qsTr("收藏")
                    onClicked: Devices.toggleFavorite(root.device.key)
                }
                // 不在线的我的设备：本机叫得醒时可以先叫醒它
                AppButton {
                    visible: root.wakeable
                    iconName: "power"
                    text: root.wakeSent ? qsTr("已叫醒") : qsTr("唤醒")
                    enabled: !root.wakeSent
                    onClicked: {
                        if (Wake.wake(root.device.deviceId))
                            root.wokenKey = root.device.key
                    }
                }
                AppButton {
                    text: qsTr("仅查看")
                    onClicked: root.connectRequested(true)
                }
                AppButton {
                    primary: true
                    text: qsTr("连接")
                    onClicked: root.connectRequested(false)
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 24
                rowSpacing: 14

                InfoItem { label: qsTr("设备 ID"); value: root.device.deviceId }
                InfoItem { label: qsTr("IP 地址"); value: root.device.ip }
                InfoItem { label: qsTr("系统"); value: root.device.os }
                InfoItem {
                    label: qsTr("上次连接")
                    value: root.device.lastSeen > 0 ? Theme.relativeTime(root.device.lastSeen) : qsTr("从未连接")
                }
                InfoItem {
                    Layout.columnSpan: 2
                    label: qsTr("网络唤醒")
                    value: !root.device.mine ? "" : root.wakeProblem.length === 0 ? qsTr("睡眠、关机以后能从本机叫醒")
                                                                                 : root.wakeProblem
                }
            }

            // 备注：输完（回车、点别处）就存
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: qsTr("备注")
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
                AppTextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("写点什么，比如这台电脑放在哪、谁在用")
                    maximumLength: 200
                    text: root.device.note
                    onEditingFinished: {
                        Devices.setNote(root.device.key, text)
                        text = Qt.binding(() => root.device.note)
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                spacing: 8
                AppButton {
                    iconName: "copy"
                    text: root.device.deviceId ? qsTr("复制设备 ID") : qsTr("复制地址")
                    onClicked: Clipboard.setText(root.device.target)
                }
                AppButton {
                    id: renameButton
                    text: qsTr("重命名")
                    onClicked: {
                        renameField.text = root.device.alias
                        renamePopup.open()
                    }
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: root.device.saved ? qsTr("移除设备") : qsTr("保存设备")
                    onClicked: root.device.saved ? Devices.forget(root.device.key) : Devices.save(root.device.key)
                }
            }
        }
    }

    // 重命名：起的名字只在本机用；清空就用回它自己的名字
    Popup {
        id: renamePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 320
        padding: 18
        modal: true
        background: PopupSurface {}
        onOpened: renameField.forceActiveFocus()

        function apply() {
            Devices.rename(root.device.key, renameField.text)
            close()
        }

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: qsTr("重命名")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.text
            }
            AppTextField {
                id: renameField
                Layout.fillWidth: true
                maximumLength: 64
                placeholderText: root.device.ownName
                onAccepted: renamePopup.apply()
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("只在本机显示；不填就用它自己的名字")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: Theme.textMuted
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                AppButton {
                    text: qsTr("取消")
                    onClicked: renamePopup.close()
                }
                AppButton {
                    primary: true
                    text: qsTr("确定")
                    onClicked: renamePopup.apply()
                }
            }
        }
    }
}
