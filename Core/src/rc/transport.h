#pragma once

#include <QJsonObject>
#include <QLatin1StringView>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>
#include <string>
#include <vector>

// 两台机器之间怎么连（docs/network.md）。SDK 要求两端用同一种传输方式，一次运行里不变：
// 观看端先直连（P2P），打不通再换成中继（TURN）重连一次。
// 去连的一方怎么证明自己（docs/devices-and-accounts.md），两端也要一样：别人用临时密码，我的设备（同一个账号下的）
// 只靠设备密钥。两端的 SDK 也得是同一个版本（1.0 以前每个版本之间都不兼容）。每份信令带着这几样
// （ConnectionMode，rc/server_protocol.h、rc/lan_protocol.h、rc/protocol.h 的 signal），分享端按它选分享会话，
// SDK 版本不一样的回绝（rc/peer_protocol.h 的 VersionMismatch）
namespace rc {

inline constexpr QLatin1StringView DirectTransport{"p2p"};
inline constexpr QLatin1StringView RelayTransport{"relay"};

// 临时密码（SDK 的 PIN）：被连的人点允许
inline constexpr QLatin1StringView PasswordAccess{"password"};
// 我的设备：只靠设备密钥（SDK 的 RCS_SECRET_NONE 加信任列表），不要密码、不用点允许
inline constexpr QLatin1StringView DeviceAccess{"device"};

bool isTransport(QStringView name);
bool isAccess(QStringView name);
// 本程序里的 SDK 的版本，比如 "0.1.0-beta"
QString sdkVersion();

// 一次连接怎么连，信令里是 "transport"、"access"、"sdk" 几个字段
struct ConnectionMode
{
    QString transport{DirectTransport};
    QString access{PasswordAccess};
    QString sdk{sdkVersion()}; // 发信令的一方的 SDK 版本

    void write(QJsonObject &message) const;
    // transport、access 都认得、带着 sdk 时才有
    static std::optional<ConnectionMode> read(const QJsonObject &message);

    friend bool operator==(const ConnectionMode &, const ConnectionMode &) = default;
};

// 中继：地址（"主机:端口"）、到它怎么走、有时效的凭据（服务器签发）
struct TurnServer
{
    QString server;
    bool tcp = false; // 到中继走 TCP（只放行 TCP 的网络）；否则 UDP
    QString username;
    QString password;

    friend bool operator==(const TurnServer &, const TurnServer &) = default;
};

// P2P 和中继用的服务器：服务器登录时发来，中继的凭据快过期前再发一份（rc/server_protocol.h 的 Welcome、Ice）；
// 后台服务经 ice 主题交给界面。都没有时（没连上过服务器）只能直连，也没有 STUN
struct IceServers
{
    QStringList stun;              // "主机:端口"：告诉每一端它的公网地址
    std::optional<TurnServer> turn; // 没有：没有中继（服务器没配，或者凭据过期了又连不上服务器）

    // SDK 要的写法
    std::vector<std::string> stunServers() const;

    // {"stun": [...], "turn": {"server", "transport": "udp" | "tcp", "username", "password"} 或 null}
    QJsonObject toJson() const;
    static IceServers fromJson(const QJsonObject &json);

    friend bool operator==(const IceServers &, const IceServers &) = default;
};

} // namespace rc
