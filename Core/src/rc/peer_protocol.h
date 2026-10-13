#pragma once

#include <cstdint>

// 两台机器之间的约定：被连的一方不让对方连进来时告诉它的原因，对方按它显示说明；
// 连上以后 SDK 数据通道（rcs_session_send_data）的通道号，收到的按它交给各自的处理者（rc/data_channels.h）
namespace rc::peer {

// ---- 数据通道 ----

// 对话：rc/chat_protocol.h
inline constexpr std::uint32_t ChatChannel = 1;
// 会话的控制：显示器、画面、声音（rc/control_protocol.h）
inline constexpr std::uint32_t ControlChannel = 2;

// ---- 不让对方连进来的原因 ----

// 密码对了，但没被允许（SDK 拒绝时的 rcs_viewer_decision.reason）
// 被控的人点了拒绝
inline constexpr char RefusedByUser[] = "refused";
// 被控电脑上没打开界面程序，没人能点允许
inline constexpr char Unattended[] = "unattended";

// 被控的一方现在根本不接受连接，经服务器回绝（rc/server_protocol.h 的 decline）
// 关着"允许别人连接本机"
inline constexpr char NotAccepting[] = "off";
// macOS 上还没授予屏幕录制
inline constexpr char NeedsPermission[] = "permission";
// 出错了，起不来
inline constexpr char Unavailable[] = "unavailable";
// 对方要的传输方式（直连、中继，rc/transport.h）和本机正在用的不一样，本机又正被别人用着，换不了
inline constexpr char TransportBusy[] = "transportBusy";
// 对方要走中继，本机没有中继（没连上服务器）
inline constexpr char NoRelay[] = "noRelay";
// 对方按我的设备来连（只靠设备密钥，rc/transport.h 的 DeviceAccess），本机不认它：本机没登录同一个账号
inline constexpr char NotMine[] = "notMine";
// 对方按我的设备来连，本机还不知道账号下有哪些设备：后台服务刚启动（开机、注销以后），服务器还没告诉它
inline constexpr char NotReady[] = "notReady";
// 两边的 SDK 不是同一个版本（rc/transport.h 的 ConnectionMode::sdk），连不上：都更新到最新版本才行
inline constexpr char VersionMismatch[] = "version";

} // namespace rc::peer
