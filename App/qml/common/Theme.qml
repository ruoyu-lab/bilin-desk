pragma Singleton
import QtQuick

// 颜色分浅色、深色两套（设置里的外观，Appearance）：每个颜色写成 dark ? 深色 : 浅色
QtObject {
    readonly property bool dark: Appearance.dark

    readonly property color bg: dark ? "#1e1e20" : "#ffffff"
    readonly property color sidebar: dark ? "#252527" : "#f7f7f9"
    readonly property color surface: dark ? "#2c2c2e" : "#ffffff"
    readonly property color inset: dark ? "#2a2a2c" : "#f0f0f3"
    readonly property color card: dark ? "#29292b" : "#f6f7fa"
    readonly property color cardHover: dark ? "#303033" : "#eff1f5"
    readonly property color selection: dark ? "#1d3557" : "#e8f1ff"
    readonly property color fill: dark ? "#333336" : "#f1f1f4"
    readonly property color fillHover: dark ? "#3a3a3d" : "#e9e9ed"
    readonly property color hover: dark ? "#14ffffff" : "#0c000000"
    readonly property color selected: dark ? "#22ffffff" : "#14000000"
    readonly property color border: dark ? "#38383b" : "#e5e5ea"
    // 设置里每一行左边小图标的底（放在 card 上）
    readonly property color tile: dark ? "#3a3a3d" : "#ffffff"
    readonly property color borderStrong: dark ? "#4a4a4d" : "#d1d1d6"
    // 选中的文字
    readonly property color textSelection: dark ? "#3a5f8f" : "#b3d7ff"
    // 弹窗后面盖住窗口的那一层
    readonly property color scrim: dark ? "#66000000" : "#26000000"
    // 开关关着时的底色
    readonly property color switchOff: dark ? "#48484b" : "#e2e2e6"

    readonly property color text: dark ? "#f2f2f4" : "#1d1d1f"
    readonly property color textSecondary: dark ? "#a1a1a6" : "#6e6e73"
    readonly property color textMuted: dark ? "#6c6c70" : "#aeaeb2"
    // 和正文颜色反过来的底（深色的按钮、勾选框）和上面的字
    readonly property color inverse: text
    readonly property color inverseHover: dark ? "#d1d1d6" : "#3a3a3c"
    readonly property color inversePressed: dark ? "#ffffff" : "#000000"
    readonly property color inverseText: dark ? "#1d1d1f" : "#ffffff"
    // 有人连着本机时屏幕顶上的刘海：深色、浅色下都是黑的
    readonly property color notch: "#1d1d1f"

    readonly property color accent: dark ? "#0a84ff" : "#007aff"
    readonly property color accentHover: dark ? "#2b93ff" : "#0a6fe0"
    readonly property color accentPressed: dark ? "#0070e0" : "#0062cc"
    readonly property color accentText: "#ffffff"

    readonly property color success: dark ? "#30d158" : "#34c759"
    readonly property color favorite: dark ? "#ffc35a" : "#ffb340"
    readonly property color warning: dark ? "#ff9f0a" : "#ff9500"
    readonly property color danger: dark ? "#ff453a" : "#ff3b30"
    readonly property color dangerHover: dark ? "#ff5e55" : "#e5352b"
    readonly property color dangerPressed: dark ? "#d93a30" : "#c92a20"
    readonly property color canvas: dark ? "#141416" : "#e6e8ec"
    // 头像（名字的第一个字）
    readonly property color avatar: dark ? "#3a3a3d" : "#e3e6ec"
    readonly property color avatarText: dark ? "#e5e5ea" : "#3a3a3c"
    // 登录窗口左边的介绍和插图
    readonly property color introBg: dark ? "#1a1b1f" : "#f4f6fb"
    readonly property color artOuter: dark ? "#24262c" : "#e9edf6"
    readonly property color artInner: dark ? "#2b2e36" : "#dfe5f2"
    readonly property color artLine: dark ? "#454b5a" : "#c3cce0"

    // macOS 挂原生统一工具栏：红绿灯与 52px 工具栏同一行；其他平台顶部留 30px 给系统标题按钮
    readonly property bool unifiedTitleBar: Qt.platform.os === "osx"
    // macOS：挂原生统一工具栏，红绿灯和工具在同一行（toolbarHeight）。
    // Windows：窗口外框交给 QWindowKit，系统标题栏去掉；工具和自己画的窗口按钮在同一行（titleBarHeight）
    readonly property int titleBarHeight: 40
    readonly property int headerHeight: unifiedTitleBar ? toolbarHeight : titleBarHeight
    readonly property int windowFlags: unifiedTitleBar ? (Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint)
                                                       : Qt.Window
    readonly property int toolbarHeight: 52
    // Windows 顶部一排按钮（工具、侧栏开关、窗口按钮）的统一边长和图标大小
    readonly property int titleButtonSize: phone ? 44 : 30
    readonly property int titleIconSize: phone ? 22 : 16
    readonly property int trafficLightsInset: 84
    readonly property int radius: 8

    // 手机、平板（触摸屏）：没有鼠标悬停，SafeArea 让开系统的东西
    readonly property bool touch: Qt.platform.os === "android" || Qt.platform.os === "ios"
    // 平板（main_mobile.cpp 按屏幕选了 TabletMain，它设上）：电脑版的布局和尺寸
    property bool tablet: false
    // 有鼠标的悬停效果：触屏上手指点完会一直留着"悬停"，那里只要按下、选中的样子（Qt 自己的控件默认也是这样）
    readonly property bool hoverEffects: !touch
    // 手机：手指点的东西做大，按钮、输入框至少 44 高，字也大一号。电脑、平板上照旧
    readonly property bool phone: touch && !tablet
    readonly property int controlHeight: phone ? 44 : 32
    readonly property int fieldHeight: phone ? 44 : 34
    readonly property int segmentHeight: phone ? 34 : 30
    readonly property int controlFont: phone ? 15 : 13
    // 列表、设置里的正文和下面的小字
    readonly property int bodyFont: phone ? 15 : 13
    readonly property int captionFont: phone ? 13 : 12
    readonly property int switchWidth: phone ? 48 : 34
    readonly property int switchHeight: phone ? 28 : 20

    function osStyle(os) {
        switch (os) {
        case "Windows": return { icon: "windows", tint: dark ? "#1b2c45" : "#e5f0ff", ink: dark ? "#5aa2ff" : "#0a6fe0" }
        case "macOS": return { icon: "apple", tint: dark ? "#323236" : "#eceef2", ink: dark ? "#d1d1d6" : "#3a3a3c" }
        case "Linux": return { icon: "terminal", tint: dark ? "#3a2a1c" : "#fff0e0", ink: dark ? "#ff9f5a" : "#e07a2f" }
        default: return { icon: "monitor", tint: dark ? "#2d2843" : "#efeafb", ink: dark ? "#a395ff" : "#6a5ad3" }
        }
    }
    readonly property int cardRadius: 12

    // 远程会话状态：needPin 等待输入密码 | connecting 连接中 | waiting 等待对方允许 | connected 已连接
    //               | reconnecting 重连中 | failed 连接失败
    //               | 让对方电脑重启、关机、睡眠、注销以后：restarting 重启中（开机后自动重连）| shutDown 已关机
    //               | asleep 已睡眠 | signingOut 注销中（接着连到登录界面）| waking 叫醒它以后等它上线
    function statusColor(status) {
        switch (status) {
        case "connected": return success
        case "needPin":
        case "shutDown":
        case "asleep": return textMuted
        case "connecting":
        case "waiting":
        case "restarting":
        case "signingOut":
        case "waking": return accent
        case "reconnecting": return warning
        default: return danger
        }
    }

    function statusLabel(status) {
        switch (status) {
        case "connected": return qsTr("已连接")
        case "needPin": return qsTr("等待输入密码")
        case "connecting": return qsTr("连接中")
        case "waiting": return qsTr("等待对方允许")
        case "reconnecting": return qsTr("重连中")
        case "restarting": return qsTr("对方重启中")
        case "shutDown": return qsTr("对方已关机")
        case "asleep": return qsTr("对方已睡眠")
        case "signingOut": return qsTr("对方注销中")
        case "waking": return qsTr("正在唤醒")
        default: return qsTr("连接失败")
        }
    }

    // 多久以前：刚刚、5 分钟前、昨天……（ms 为毫秒时间戳）
    function relativeTime(ms) {
        if (ms <= 0)
            return ""
        const minutes = Math.floor((Date.now() - ms) / 60000)
        if (minutes < 1)
            return qsTr("刚刚")
        if (minutes < 60)
            return qsTr("%1 分钟前").arg(minutes)
        const hours = Math.floor(minutes / 60)
        if (hours < 24)
            return qsTr("%1 小时前").arg(hours)
        const days = Math.floor(hours / 24)
        return days === 1 ? qsTr("昨天") : qsTr("%1 天前").arg(days)
    }

    // 字节数的显示：1.2 MB 这样
    function formatBytes(bytes) {
        if (bytes < 1024)
            return bytes + " B"
        const units = ["KB", "MB", "GB", "TB"]
        let value = bytes / 1024
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            unit++
        }
        return (value >= 100 ? value.toFixed(0) : value.toFixed(1)) + " " + units[unit]
    }
}
