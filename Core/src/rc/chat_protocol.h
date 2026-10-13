#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <variant>

// 对话通道（rc/peer_protocol.h 的 ChatChannel）上的消息，一条是一个 JSON 对象（UTF-8，SDK 的可靠消息）：
//   {"type": "text", "text": 文字}           说一句话，两个方向都有
//   {"type": "presence", "attended": bool}   被控的一方告诉观看端：它那边开着界面程序没有（没开着时说的话没人看）。
//                                            观看端连进来时发一次，之后变了再发
// 两端是同一个版本的程序（SDK 本来就要求两端同一个版本）
namespace rc::chat {

// 一句话最长多少（QString 的长度，和输入框的上限一样）
inline constexpr qsizetype MaxText = 4000;

struct Text
{
    QString text;

    friend bool operator==(const Text &, const Text &) = default;
};

struct Presence
{
    bool attended = false;

    friend bool operator==(const Presence &, const Presence &) = default;
};

using Message = std::variant<Text, Presence>;

QByteArray text(const QString &text);
QByteArray presence(bool attended);
// 格式不对的、没有字的、太长的：nullopt
std::optional<Message> parse(const QByteArray &data);

} // namespace rc::chat
