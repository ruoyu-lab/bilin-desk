#pragma once

#include <QJsonObject>
#include <QLatin1StringView>
#include <QList>
#include <QString>

// 界面程序和后台服务之间本机连接上的消息，一条消息是一个 JSON 对象：
//   后台服务 → 界面：{"topic": 主题, "value": {...}}。界面连上时每个主题发一遍，之后有变化再发整个主题
//                    {"event": 事件, ...参数}：一次性的通知，没连着的界面就收不到
//   界面 → 后台服务：{"command": 命令, ...参数}
// 两边是同一次编译出来的程序（界面发现后台服务不是旁边那一份时会换掉它），改了就两边一起改
namespace rc::protocol {

// ---- 主题 ----

// 本机：DeviceState
inline constexpr QLatin1StringView DeviceTopic{"device"};
// 后台服务这个程序：ServiceInfo
inline constexpr QLatin1StringView ServiceTopic{"service"};
// 别人连本机：ShareState
inline constexpr QLatin1StringView ShareTopic{"share"};
// 局域网里附近的设备：NearbyState
inline constexpr QLatin1StringView NearbyTopic{"nearby"};
// P2P 和中继用的服务器：rc::IceServers（rc/transport.h），服务器发来的
inline constexpr QLatin1StringView IceTopic{"ice"};
// 本机登录的账号、账号下的设备：AccountState
inline constexpr QLatin1StringView AccountTopic{"account"};
// 别人连本机时互传的文件：{"transfers": [...]}，每一项见 rc/transfer_list.h 的 Transfer（id 用在下面的命令里）
inline constexpr QLatin1StringView FilesTopic{"files"};
// 本机的连接记录（HistoryState）：谁什么时候连过本机、被拒绝的、输错密码的、互传的文件、程序更新了
inline constexpr QLatin1StringView HistoryTopic{"history"};
// 程序的新版本（UpdateState）：只有装好的程序（后台服务是系统服务）有，开发时编出来的不发
inline constexpr QLatin1StringView UpdateTopic{"update"};

// ---- 命令 ----

// name：空的表示用回默认的名字
inline constexpr QLatin1StringView SetDeviceName{"setDeviceName"};
// value
inline constexpr QLatin1StringView SetAcceptIncoming{"setAcceptIncoming"};
// value
inline constexpr QLatin1StringView SetLanDiscovery{"setLanDiscovery"};
// value：装好了系统服务时，界面程序没开着也接受我的设备连接（DeviceState 的 unattended）
inline constexpr QLatin1StringView SetUnattended{"setUnattended"};
inline constexpr QLatin1StringView RegeneratePassword{"regeneratePassword"};
// 后台服务退出（托盘里的"退出"；换成另一份程序）
inline constexpr QLatin1StringView Quit{"quit"};
// 界面程序要退出了（本机的人确认过）：断开连着本机的人；之后界面没了不用把它拉起来
inline constexpr QLatin1StringView AppQuitting{"appQuitting"};
// id、control：让请求连接本机的人连进来，control 是能不能控制本机
inline constexpr QLatin1StringView Admit{"admit"};
// id：不让请求连接本机的人连进来
inline constexpr QLatin1StringView Refuse{"refuse"};
// id：断开连着本机的人
inline constexpr QLatin1StringView Disconnect{"disconnect"};
// id、value：让连着本机的人能不能控制
inline constexpr QLatin1StringView SetControl{"setControl"};
// value：和连着本机的人同步不同步剪贴板（所有人一起，share 主题的 clipboard）
inline constexpr QLatin1StringView SetClipboard{"setClipboard"};
// permission（ScreenPermission | InputPermission）：弹出系统的授权提示（macOS 的屏幕录制、辅助功能；只有第一次会弹）
inline constexpr QLatin1StringView RequestPermission{"requestPermission"};
inline constexpr QLatin1StringView ScreenPermission{"screen"};
inline constexpr QLatin1StringView InputPermission{"input"};
// 出了错、别人连不上本机时再试一次
inline constexpr QLatin1StringView RestartShare{"restartShare"};
// jpeg（base64）：本机桌面的背景图，发给连着本机的人（rc/control_protocol.h 的 wallpaper）。后台服务以 SYSTEM、
// root 运行，读不到登录的人的背景：有人连进来时它要（WallpaperWanted），界面程序读好发过来
inline constexpr QLatin1StringView SetWallpaper{"setWallpaper"};
// email、purpose：往邮箱发验证码（注册、找回密码），发了有 codeSent
inline constexpr QLatin1StringView SendCode{"sendCode"};
inline constexpr QLatin1StringView RegisterPurpose{"register"};
inline constexpr QLatin1StringView ResetPasswordPurpose{"resetPassword"};
// email、password、nickname、code（发到邮箱的验证码）：注册账号，本机随即登录它
inline constexpr QLatin1StringView RegisterAccount{"registerAccount"};
// email、password
inline constexpr QLatin1StringView SignIn{"signIn"};
// email、code、password：忘了密码，凭发到邮箱的验证码设新密码，本机随即登录它
inline constexpr QLatin1StringView ResetPassword{"resetPassword"};
inline constexpr QLatin1StringView SignOut{"signOut"};
// deviceId：把账号下的另一台设备移出账号
inline constexpr QLatin1StringView RemoveDevice{"removeDevice"};
// 清空本机的连接记录
inline constexpr QLatin1StringView ClearHistory{"clearHistory"};
// 下载、安装新版本（update 主题里的 available）。装的时候后台服务、界面程序都会被关掉，装完再打开
inline constexpr QLatin1StringView InstallUpdate{"installUpdate"};
// 马上问服务器有没有新版本（设置里的"检查更新"），结果在 update 主题的 checked
inline constexpr QLatin1StringView CheckUpdate{"checkUpdate"};
// nickname：改登录着的账号的昵称
inline constexpr QLatin1StringView SetNickname{"setNickname"};
// oldPassword、newPassword：改登录着的账号的密码，成了有 passwordChanged
inline constexpr QLatin1StringView ChangePassword{"changePassword"};
// password：注销登录着的账号（账号删掉，挂在下面的设备都不再属于它）
inline constexpr QLatin1StringView DeleteAccount{"deleteAccount"};
// to、address、session、text、transport、access、sdk：界面里去连别人的会话（观看端）的信令，交给设备 to。
// 有 address（按 IP 地址连，这时 to 可以为空）、或者 to 就在附近（局域网）时直接走局域网，否则经服务器。
// session 是界面给这次连接起的编号，对方的回答带着它回来；transport、access、sdk 是这次连接怎么连（rc/transport.h）
inline constexpr QLatin1StringView Signal{"signal"};
// session：界面里的这次连接结束了，它的信令不再转。连着的时候哪一端换了网络都会再发信令（SDK 重新找路），
// 所以后台服务一直记着它，直到界面说结束（或者界面程序断开）
inline constexpr QLatin1StringView EndSignal{"endSignal"};
// id、ref、text：对连着本机的人 id 说一句话（对话）。ref 是界面给这一句起的编号，没发出去时 chatFailed 带着它回来
inline constexpr QLatin1StringView SendChat{"sendChat"};
// viewer、paths：把本机的这些文件、文件夹（绝对路径）发给连着本机的人 viewer（share 主题里的 id）
inline constexpr QLatin1StringView SendFiles{"sendFiles"};
// id、directory：收下别人发来的（files 主题里 offered 的），存到 directory（本机已有的文件夹，绝对路径）
inline constexpr QLatin1StringView AcceptTransfer{"acceptTransfer"};
// id：不收
inline constexpr QLatin1StringView RejectTransfer{"rejectTransfer"};
// id：取消没结束的
inline constexpr QLatin1StringView CancelTransfer{"cancelTransfer"};
// id：再发一次没发成的
inline constexpr QLatin1StringView RetryTransfer{"retryTransfer"};
// id：从列表里去掉结束了的
inline constexpr QLatin1StringView RemoveTransfer{"removeTransfer"};

// ---- 事件 ----

// from、session、text：设备 from 对界面某个会话的信令的回答（事件名和命令一样是 signal）
// from、session、reason：设备 from 现在不接受连接，回绝了（reason 见 rc/peer_protocol.h）
inline constexpr QLatin1StringView Declined{"declined"};
// reason：发验证码、注册、登录、找回密码、改密码、注销没成功。emailTaken（邮箱注册过了）| noAccount（邮箱没注册过）
// | wrongPassword（邮箱或密码不对）| wrongCode（验证码不对、过期了）| tooMany（试错太多次、要验证码太频繁，等一会儿）
// | mailLimit（服务器今天发的邮件到上限了）| mailFailed（邮件没发出去）| noServer（本机没连上服务器）
inline constexpr QLatin1StringView AccountFailed{"accountFailed"};
// 改密码成了
inline constexpr QLatin1StringView PasswordChanged{"passwordChanged"};
// 有人连进本机了：界面程序读一下本机桌面的背景发过来（SetWallpaper）
inline constexpr QLatin1StringView WallpaperWanted{"wallpaperWanted"};
// purpose：验证码发到邮箱了
inline constexpr QLatin1StringView CodeSent{"codeSent"};
// to、session、reason：信令没送到。reason：offline（设备 to 不在线，或者没有这个 ID）| noServer（本机没连上服务器）
// | unreachable（局域网里连不上那台电脑：地址不对，或者对方没打开本程序）| tooMany（发起的连接、连不上的太多，
// 服务器先不让连）
inline constexpr QLatin1StringView Undeliverable{"undeliverable"};
inline constexpr QLatin1StringView Offline{"offline"};
inline constexpr QLatin1StringView TooMany{"tooMany"};
inline constexpr QLatin1StringView NoServer{"noServer"};
inline constexpr QLatin1StringView Unreachable{"unreachable"};
// id、text：连着本机的人 id 说了一句话
inline constexpr QLatin1StringView ChatReceived{"chatReceived"};
// id、ref、reason：对连着本机的人 id 说的话（sendChat）没发出去。reason：queueFull（排着要发给对方的太多了，
// 过一会儿再说）| notConnected（对方断开了，或者正在重连）
inline constexpr QLatin1StringView ChatFailed{"chatFailed"};
inline constexpr QLatin1StringView QueueFull{"queueFull"};
inline constexpr QLatin1StringView NotConnected{"notConnected"};

struct DeviceState
{
    QString deviceId; // 9 位数字
    QString deviceName;
    QString password; // 临时密码
    bool acceptIncoming = true;
    bool lanDiscovery = true;
    // 界面程序没开着时也接受我的设备连接（docs/install-and-first-run.md 的"开机时自动启动"）。开发版的后台服务
    // 跟着界面程序，装好的系统服务一直在，这个说它没人开界面时做不做事
    bool unattended = true;
    // 设备密钥（SDK 的 P-256 密钥对，97 字节）：界面去连我的设备时用它证明自己（rc/transport.h 的 DeviceAccess）
    QByteArray deviceKey;

    QJsonObject toJson() const;
    static DeviceState fromJson(const QJsonObject &json);

    friend bool operator==(const DeviceState &, const DeviceState &) = default;
};

// 连本机的一个人：id 是这次分享里的编号，platform 是对方的系统（macOS、Windows……），
// control 是能不能控制本机（请求连接时没有意义）
struct SharePeer
{
    int id = 0;
    QString deviceId;
    QString name;
    QString platform;
    bool control = false;

    QJsonObject toJson() const;
    static SharePeer fromJson(const QJsonObject &json);

    friend bool operator==(const SharePeer &, const SharePeer &) = default;
};

// 别人连本机：现在能不能被连、缺什么，等本机的人点允许的，已经连着的
struct ShareState
{
    // 别人现在能连本机：开着"允许别人连接本机"，也起来了
    bool accepting = false;
    // 开着"允许别人连接本机"却连不上时为什么（rc/peer_protocol.h）：permission（缺屏幕录制）| unavailable（出错了）
    QString problem;
    // macOS 的"屏幕与系统音频录制"和"辅助功能"授权；Windows 上总是有
    bool screenPermission = false;
    bool inputPermission = false;
    // 现在能让对方控制本机（没有辅助功能授权时只能让对方看）
    bool canControl = false;
    // 和连着本机的人同步剪贴板（本机的人可以关掉，所有人一起；对方那一面也开着才同步）
    bool clipboard = true;
    QList<SharePeer> requests;
    QList<SharePeer> viewers;

    QJsonObject toJson() const;
    static ShareState fromJson(const QJsonObject &json);

    friend bool operator==(const ShareState &, const ShareState &) = default;
};

// 局域网里附近的一台设备（它开着"发现附近的设备"）：address 是它的 IPv4 地址
struct NearbyDevice
{
    QString deviceId;
    QString name;
    QString platform;
    QString address;

    QJsonObject toJson() const;
    static NearbyDevice fromJson(const QJsonObject &json);

    friend bool operator==(const NearbyDevice &, const NearbyDevice &) = default;
};

// 附近的设备；本机关着"发现附近的设备"时是空的
struct NearbyState
{
    QList<NearbyDevice> devices;

    QJsonObject toJson() const;
    static NearbyState fromJson(const QJsonObject &json);

    friend bool operator==(const NearbyState &, const NearbyState &) = default;
};

// 账号下的一台设备。publicKey：设备密钥的公钥（base64），用来认出它
struct AccountDevice
{
    QString deviceId;
    QString name;
    QString platform;
    QString publicKey;
    bool online = false;

    QJsonObject toJson() const;
    static AccountDevice fromJson(const QJsonObject &json);

    friend bool operator==(const AccountDevice &, const AccountDevice &) = default;
};

// 本机登录的账号；没登录时 signedIn 为 false，其余都是空的
struct AccountState
{
    bool signedIn = false;
    QString email;
    QString nickname;
    QList<AccountDevice> devices; // 包括本机

    QJsonObject toJson() const;
    static AccountState fromJson(const QJsonObject &json);

    friend bool operator==(const AccountState &, const AccountState &) = default;
};

// 连接记录里的一条（时间是 UTC 的毫秒）。kind：
//   connection  设备 deviceId（名字 name）连进来了，从 at 到 endedAt（0：还连着），control：能控制本机
//   refused     它请求连接没成：reason 是 refused（本机的人拒绝了）| timeout（一直没人点）| unattended（没人能点）
//   wrongPin    有人输错了临时密码（不知道是谁）
//   file        和它互传的文件：file 是第一项的名字（count 项），bytes 是大小，sent：本机发出去的
//   updated     程序自己更新到了 version（docs/admin.md）
//   power       我的设备 deviceId 让本机 action（restart | shutDown | sleep | signOut）
struct HistoryEntry
{
    int id = 0;
    QString kind;
    QString deviceId;
    QString name;
    qint64 at = 0;
    qint64 endedAt = 0;
    bool control = false;
    QString reason;
    QString file;
    int count = 0;
    qint64 bytes = 0;
    bool sent = false;
    QString version;
    QString action;

    QJsonObject toJson() const;
    static HistoryEntry fromJson(const QJsonObject &json);

    friend bool operator==(const HistoryEntry &, const HistoryEntry &) = default;
};

// 连接记录，新的在前
struct HistoryState
{
    QList<HistoryEntry> entries;

    QJsonObject toJson() const;
    static HistoryState fromJson(const QJsonObject &json);

    friend bool operator==(const HistoryState &, const HistoryState &) = default;
};

// 程序的新版本（docs/admin.md）：available 是服务器上发布的、比本机新的版本（没有时为空）；
// phase：idle | checking（设置里点了检查更新）| downloading | installing；progress：下载了多少（0~1）；
// failed：上一次没成的那一步 download（没下载对）| install（装了却没换上，或者安装程序启动不了）| 空；
// checked：点了检查更新的结果 upToDate（已是最新）| failed（问不了服务器）| 空（没点过，或者有新版本）
struct UpdateState
{
    QString available;
    QString phase = QStringLiteral("idle");
    double progress = 0;
    QString failed;
    QString checked;

    QJsonObject toJson() const;
    static UpdateState fromJson(const QJsonObject &json);

    friend bool operator==(const UpdateState &, const UpdateState &) = default;
};

// 正在运行的后台服务是哪一份程序：界面据此判断它是不是自己旁边那一份、有没有重新编译过或升级过
struct ServiceInfo
{
    QString program;     // 程序文件
    qint64 modified = 0; // 它启动时程序文件的修改时间（毫秒）

    QJsonObject toJson() const;
    static ServiceInfo fromJson(const QJsonObject &json);

    friend bool operator==(const ServiceInfo &, const ServiceInfo &) = default;
};

QJsonObject topicMessage(QLatin1StringView topic, const QJsonObject &value);
QJsonObject commandMessage(QLatin1StringView command, const QJsonObject &arguments = {});
QJsonObject eventMessage(QLatin1StringView event, const QJsonObject &arguments);

} // namespace rc::protocol
