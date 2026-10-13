#pragma once

#include "rc/transport.h"

#include <QByteArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QString>

#include <optional>

// 局域网里本程序之间的消息，不经服务器：
// - 发现（UDP 广播，DiscoveryPort）：开着"发现附近的设备"的电脑每 AnnounceMs 往每块网卡的广播地址发一次 announce
//   {"app": AppTag, "type": "announce", "deviceId", "name", "platform"}；关掉、退出时发一次 leave
//   {"app", "type": "leave", "deviceId"}。ForgetMs 没听到的不再算在附近
// - 信令（TCP，SignalingPort，一行一个 JSON，rc/line_channel.h）：去连的一方连到对方的这个端口发 signal
//   {"type": "signal", "from", "name", "session", "text", "transport", "access", "sdk"}（rc/transport.h），被连的一方
//   在同一条连接上回 signal（同样的字段），或者现在不接受连接时回 decline {"type": "decline", "from", "session", "reason"}
//   （原因见 rc/peer_protocol.h）
// SDK 要求两端是同一个版本：sdk 和本机的不一样时，被连的一方回绝（rc/peer_protocol.h 的 VersionMismatch）
namespace rc::lan {

inline constexpr quint16 DiscoveryPort = 47920;
inline constexpr quint16 SignalingPort = 47921;
inline constexpr int AnnounceMs = 2000;
inline constexpr int ForgetMs = 7000;

inline constexpr QLatin1StringView AppTag{"bilin"};
inline constexpr QLatin1StringView Announce{"announce"};
inline constexpr QLatin1StringView Leave{"leave"};
inline constexpr QLatin1StringView Signal{"signal"};
inline constexpr QLatin1StringView Decline{"decline"};

// 发现：一台电脑说"我在"（name、platform 为空表示它说"我走了"）
struct Presence
{
    QString deviceId;
    QString name;
    QString platform;

    friend bool operator==(const Presence &, const Presence &) = default;
};

QByteArray announce(const Presence &presence);
QByteArray leave(const QString &deviceId);
// 不是本程序发的、格式不对的：nullopt；leave 的 name 为空
std::optional<Presence> parsePresence(const QByteArray &datagram);

QJsonObject signal(const QString &from, const QString &name, const QString &session, const QString &text,
                   const rc::ConnectionMode &mode);
QJsonObject decline(const QString &from, const QString &session, const QString &reason);

} // namespace rc::lan
