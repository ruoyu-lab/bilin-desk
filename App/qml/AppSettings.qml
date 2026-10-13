pragma Singleton
import QtQuick
import QtCore

// 本地持久化的偏好设置
Settings {
    // 收到的文件存到哪个文件夹；空的时候是系统的"下载"
    property url downloadFolder

    // 会话窗口里上次的选择：侧边面板开着没有、剪贴板同步、远程声音
    property bool sessionPanelOpen: false
    property bool clipboardSync: true
    property bool remoteAudio: true

    // 会话里选的分辨率（长边的像素，0 是原始分辨率）、帧率（0 是对方显示器的刷新率）、码率（Mbit/s，0 是自动）、
    // 编码格式（0 是自动），连哪台都用，不超过那一台的上限（view_quality.h）；显示不显示对方的指针
    property int sessionResolution: 1920
    property int sessionFrameRate: 60
    property int sessionBitrate: 0
    property int sessionCodec: 0
    // 文字更清楚（4:4:4，两边都做得了才用）、低延迟模式
    property bool sessionClearText: false
    property bool sessionLowLatency: false
    property bool remoteCursor: true

    // macOS 第一次打开时的授权引导走完了（完成、以后再说），之后不再出现
    property bool permissionGuideDone: false

    // 被控时连接请求卡片上次拖到的位置（窗口左上角，逻辑像素）；-1 表示还没拖过
    property int requestX: -1
    property int requestY: -1
}
