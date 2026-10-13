import QtQuick
import QtQuick.Layouts

Flickable {
    id: root

    // 这个面板属于哪个会话（SessionView）
    required property Item view
    readonly property ViewerSession session: view.session

    contentWidth: width
    contentHeight: column.height + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    component GroupTitle: Text {
        Layout.topMargin: 6
        font.pixelSize: Theme.captionFont
        font.weight: Font.Medium
        color: Theme.textMuted
    }

    component SwitchRow: RowLayout {
        id: switchRow
        property alias label: name.text
        property alias checked: toggle.checked
        signal toggled()
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        Text {
            id: name
            Layout.fillWidth: true
            font.pixelSize: Theme.bodyFont
            color: Theme.text
        }
        AppSwitch { id: toggle; onToggled: switchRow.toggled() }
    }

    // 开关下面的一句说明：开着却同步不了、听不到时为什么
    component NoteText: Text {
        Layout.fillWidth: true
        Layout.bottomMargin: 4
        visible: text.length > 0
        wrapMode: Text.Wrap
        font.pixelSize: Theme.captionFont
        color: Theme.textSecondary
    }

    component InfoRow: RowLayout {
        property alias label: name.text
        property alias value: data.text
        Layout.fillWidth: true
        Layout.preferredHeight: 26
        Text {
            id: name
            Layout.fillWidth: true
            font.pixelSize: Theme.bodyFont
            color: Theme.textSecondary
        }
        Text {
            id: data
            font.pixelSize: Theme.bodyFont
            color: Theme.text
        }
    }

    // 一行：左边名字，右边下拉框
    component ChoiceRow: RowLayout {
        id: choiceRow
        property alias label: name.text
        property alias model: box.model
        property alias currentIndex: box.currentIndex
        signal activated(int index)
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        spacing: 12
        Text {
            id: name
            Layout.preferredWidth: 48
            font.pixelSize: Theme.bodyFont
            color: Theme.text
        }
        AppComboBox {
            id: box
            Layout.fillWidth: true
            onActivated: index => choiceRow.activated(index)
        }
    }

    ColumnLayout {
        id: column
        x: 16
        y: 16
        width: root.width - 32
        spacing: 8

        GroupTitle { text: qsTr("画面") }
        // 对方的显示器：一次看一块（工具栏上也能换）
        ChoiceRow {
            readonly property var displays: root.session ? root.session.displays : null
            visible: displays !== null && displays.items.length > 0
            enabled: displays !== null && displays.items.length > 1 && root.session.controlGranted
            label: qsTr("显示器")
            model: displays ? displays.items.map((d, i) => ({
                label: (d.name.length > 0 ? d.name : qsTr("显示器 %1").arg(i + 1))
                       + (d.main ? qsTr("（主）") : "") + "　" + d.width + " × " + d.height,
                value: i,
            })) : []
            currentIndex: displays ? displays.currentIndex : -1
            onActivated: index => displays.show(index)
        }
        // 换了马上生效，不用重连；下次连别人也用它（不超过那一台的显示器）
        ChoiceRow {
            visible: root.session !== null && root.session.resolutionOptions.length > 0
            label: qsTr("分辨率")
            model: root.session ? root.session.resolutionOptions : []
            currentIndex: root.session ? root.session.resolutionIndex : -1
            onActivated: index => {
                const value = root.session.resolutionOptions[index].value
                root.session.resolutionChoice = value
                AppSettings.sessionResolution = value
            }
        }
        ChoiceRow {
            visible: root.session !== null && root.session.resolutionOptions.length > 0
            label: qsTr("帧率")
            model: root.session ? root.session.frameRateOptions : []
            currentIndex: root.session ? root.session.frameRateIndex : -1
            onActivated: index => {
                const value = root.session.frameRateOptions[index].value
                root.session.frameRateChoice = value
                AppSettings.sessionFrameRate = value
            }
        }
        ChoiceRow {
            visible: root.session !== null && root.session.resolutionOptions.length > 0
            label: qsTr("码率")
            model: root.session ? root.session.bitrateOptions : []
            currentIndex: root.session ? root.session.bitrateIndex : -1
            onActivated: index => {
                const value = root.session.bitrateOptions[index].value
                root.session.bitrateChoice = value
                AppSettings.sessionBitrate = value
            }
        }
        // 两边都能用的；两边都用显卡的有就只列它们，没有时才列要用 CPU 的
        ChoiceRow {
            visible: root.session !== null && root.session.codecOptions.length > 0
            label: qsTr("编码器")
            model: root.session ? root.session.codecOptions : []
            currentIndex: root.session ? root.session.codecIndex : -1
            onActivated: index => {
                const value = root.session.codecOptions[index].value
                root.session.codecChoice = value
                AppSettings.sessionCodec = value
            }
        }
        // 两边都用显卡做得了 4:4:4 的编码格式时才有；打开时编码器只列这几种
        SwitchRow {
            visible: root.session !== null && root.session.clearTextAvailable
            label: qsTr("文字更清楚")
            checked: root.session ? root.session.clearText : false
            onToggled: {
                root.session.clearText = checked
                AppSettings.sessionClearText = checked
            }
        }
        NoteText {
            text: root.session && root.session.clearTextAvailable
                  ? qsTr("颜色精细到每个像素，看代码、表格时字边不糊；码率高一些") : ""
        }
        SwitchRow {
            visible: root.session !== null
            label: qsTr("低延迟模式")
            checked: root.session ? root.session.lowLatency : false
            onToggled: {
                root.session.lowLatency = checked
                AppSettings.sessionLowLatency = checked
            }
        }
        NoteText {
            text: root.session ? qsTr("操作以后画面跟得更快，适合在同一个网络里用") : ""
        }
        // 对方同时被几个人连着：画面按先连上的那个人要的出
        NoteText {
            text: root.session && root.session.status === "connected" && !root.session.mainViewer
                  ? qsTr("对方同时被别人连着，画面由先连上的人定") : ""
        }
        // 手机上用两个手指放大缩小（RemoteView），没有这一项
        Text { visible: !Theme.touch; Layout.topMargin: 4; text: qsTr("缩放"); font.pixelSize: Theme.bodyFont; color: Theme.text }
        SegmentedControl {
            visible: !Theme.touch
            Layout.fillWidth: true
            model: [qsTr("适应窗口"), qsTr("原始大小")]
            currentIndex: root.view.nativeSize ? 1 : 0
            onCurrentIndexChanged: root.view.nativeSize = currentIndex === 1
        }

        GroupTitle { Layout.topMargin: 14; text: qsTr("同步") }
        SwitchRow {
            label: qsTr("剪贴板同步")
            checked: root.session ? root.session.clipboard : false
            onToggled: {
                root.session.clipboard = checked
                AppSettings.clipboardSync = checked
            }
        }
        NoteText {
            text: root.session && root.session.status === "connected" && root.session.clipboard
                  ? root.session.clipboardNote : ""
        }
        SwitchRow {
            label: qsTr("远程声音")
            checked: root.session ? root.session.audio : false
            onToggled: {
                root.session.audio = checked
                AppSettings.remoteAudio = checked
            }
        }
        NoteText {
            text: root.session && root.session.status === "connected" && root.session.audio
                  ? root.session.audioNote : ""
        }
        // 手机上没有本机的指针，对方的指针一直画着
        SwitchRow {
            visible: !Theme.touch
            label: qsTr("显示远程光标")
            checked: AppSettings.remoteCursor
            onToggled: AppSettings.remoteCursor = checked
        }

        GroupTitle { Layout.topMargin: 14; text: qsTr("连接信息") }
        InfoRow {
            readonly property bool live: root.session !== null && root.session.status === "connected"
            label: qsTr("连接方式")
            value: live && root.session.connectionType.length > 0 ? root.session.connectionType : "—"
        }
        InfoRow {
            readonly property bool live: root.session !== null && root.session.status === "connected"
            label: qsTr("延迟")
            value: live ? qsTr("%1 ms").arg(root.session.latencyMs) : "—"
        }
        InfoRow {
            label: qsTr("码率")
            value: root.session && root.session.status === "connected"
                   ? qsTr("%1 Mbps").arg(root.session.bitrateMbps.toFixed(1)) : "—"
        }
        InfoRow {
            label: qsTr("帧率")
            value: root.session && root.session.status === "connected" ? qsTr("%1 FPS").arg(root.session.framesPerSecond) : "—"
        }
        InfoRow {
            label: qsTr("分辨率")
            value: root.session && root.session.resolution.length > 0 ? root.session.resolution : "—"
        }
        InfoRow {
            label: qsTr("编码")
            value: root.session && root.session.codec.length > 0 ? root.session.codec : "—"
        }
    }
}
