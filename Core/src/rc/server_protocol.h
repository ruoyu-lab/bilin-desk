#pragma once

#include "rc/transport.h"

#include <QJsonObject>
#include <QLatin1StringView>
#include <QString>
#include <QStringView>

// 后台服务和服务器之间的消息：WebSocket 上一条文字消息是一个 JSON 对象，type 说明是哪一种。
// 服务器那一边在 Server/src/protocol.ts，改了两边一起改。
// 连上以后先发 hello，服务器回 welcome 才算登录；ID 已经属于别的设备时回 rejected（idTaken），换一个再来。
// 登录以后发 signal（带着这次连接怎么连：传输方式、怎么证明自己，rc/transport.h）、现在不接受别人连本机时回 decline、
// 改了名字发 rename。
// 账号（邮箱 + 密码）：注册、找回密码先 sendCode，服务器往邮箱发 6 位验证码，回 codeSent；registerAccount（带验证码）、
// signIn、resetPassword（凭验证码设新密码）把本机挂到账号下，signOut 拿下来，removeDevice 把账号下别的设备拿下来；
// 服务器随即（以后有变化时也）发来 account 和 devices。登录着时 setNickname 改昵称，changePassword 改密码（要旧密码，
// 成了回 passwordChanged），deleteAccount 注销账号（要密码，成了回 account 为 null）。
// 服务器限速：试得太多时 rejected、accountFailed、undeliverable 的 reason 是 tooMany
namespace rc::server {

// ---- 服务器 → 客户端 ----
// ice：P2P 和中继用的服务器（rc::IceServers 的 JSON，另有 ttl：中继的凭据还能用多少秒），服务器没配中继时为 null
inline constexpr QLatin1StringView Welcome{"welcome"};
// ice：中继的新凭据（同上），旧的过了一半时效时发来
inline constexpr QLatin1StringView Ice{"ice"};
// reason：idTaken | invalid | timeout | tooMany（这个地址登记的新设备太多，过一会儿再来）
inline constexpr QLatin1StringView Rejected{"rejected"};
inline constexpr QLatin1StringView IdTaken{"idTaken"};
// from、name、session、text、transport、access、sdk：别的设备（ID 和名字）发来的信令
inline constexpr QLatin1StringView Signal{"signal"};
// from、session、reason：设备 from 现在不接受连接，回绝了这次连接（原因见 rc/peer_protocol.h）
inline constexpr QLatin1StringView Decline{"decline"};
// account：{email, nickname}，没登录账号时为 null
inline constexpr QLatin1StringView Account{"account"};
// reason：emailTaken | noAccount | wrongPassword | wrongCode | tooMany | mailLimit | mailFailed（Server/src/protocol.ts）
inline constexpr QLatin1StringView AccountFailed{"accountFailed"};
inline constexpr QLatin1StringView EmailTaken{"emailTaken"};
inline constexpr QLatin1StringView WrongPassword{"wrongPassword"};
inline constexpr QLatin1StringView TooMany{"tooMany"};
// 改密码成了
inline constexpr QLatin1StringView PasswordChanged{"passwordChanged"};
// purpose：验证码发到邮箱了
inline constexpr QLatin1StringView CodeSent{"codeSent"};
// devices：[{deviceId, name, platform, publicKey, online}]，账号下所有的设备（包括本机）
inline constexpr QLatin1StringView Devices{"devices"};
// to、session、reason：信令没送到。reason：offline（对方不在线，或者没有这个 ID）| tooMany（本机发起的连接、
// 连不上的太多了，过一会儿再连）
inline constexpr QLatin1StringView Undeliverable{"undeliverable"};
// 这台设备从别处又登录了一次：这条连接到此为止
inline constexpr QLatin1StringView Replaced{"replaced"};
// deviceId：登录账号时服务器认出本机是账号下原来的那一台（重装过，rc/machine.h）。那个 ID 已经换成了本机的令牌、
// 公钥：换回它、用同一个令牌重新登录。服务器随后关掉这条连接
inline constexpr QLatin1StringView Renumbered{"renumbered"};

// 服务器收的名字最长 64 个字
inline constexpr qsizetype MaxNameLength = 64;

// 设备令牌：证明"这个设备 ID 是我的"，32 字节随机数的 base64url（不带"="）。只存在本机，服务器只存它的哈希
QString generateDeviceToken();
bool isDeviceToken(QStringView text);

// 本机的系统，服务器上记着给对方看：macOS、Windows……
QString platformName();

// publicKey：设备密钥的公钥（65 字节）；machine：rc::machineHash()
QJsonObject hello(const QString &deviceId, const QString &token, const QByteArray &publicKey, const QString &name,
                  const QString &version, const QString &machine);
// 给设备 to 的信令；session 是发起连接的一方给这次连接起的编号，回答时原样带回；mode 是这次连接怎么连
QJsonObject signal(const QString &to, const QString &session, const QString &text, const rc::ConnectionMode &mode);
// 回绝设备 to 的这次连接
QJsonObject decline(const QString &to, const QString &session, const QString &reason);
// 本机改了名字：之后发出的信令带新名字
QJsonObject rename(const QString &name);
// purpose：rc/protocol.h 的 RegisterPurpose | ResetPasswordPurpose
QJsonObject sendCode(const QString &email, const QString &purpose);
QJsonObject registerAccount(const QString &email, const QString &password, const QString &nickname, const QString &code);
QJsonObject signIn(const QString &email, const QString &password);
QJsonObject resetPassword(const QString &email, const QString &code, const QString &password);
QJsonObject signOut();
QJsonObject removeDevice(const QString &deviceId);
QJsonObject setNickname(const QString &nickname);
QJsonObject changePassword(const QString &oldPassword, const QString &newPassword);
QJsonObject deleteAccount(const QString &password);

} // namespace rc::server
