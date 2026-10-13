#include "rc/chat_protocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>

namespace rc::chat {

namespace {

constexpr QLatin1StringView TextType{"text"};
constexpr QLatin1StringView PresenceType{"presence"};

QByteArray encode(const QJsonObject &message)
{
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

} // namespace

QByteArray text(const QString &text)
{
    return encode({{QStringLiteral("type"), QString(TextType)}, {QStringLiteral("text"), text}});
}

QByteArray presence(bool attended)
{
    return encode({{QStringLiteral("type"), QString(PresenceType)}, {QStringLiteral("attended"), attended}});
}

std::optional<Message> parse(const QByteArray &data)
{
    const QJsonObject message = QJsonDocument::fromJson(data).object();
    const QString type = message.value(QLatin1StringView("type")).toString();
    if (type == TextType) {
        QString words = message.value(QLatin1StringView("text")).toString();
        if (words.trimmed().isEmpty() || words.size() > MaxText)
            return std::nullopt;
        return Text{std::move(words)};
    }
    if (type == PresenceType)
        return Presence{message.value(QLatin1StringView("attended")).toBool()};
    return std::nullopt;
}

} // namespace rc::chat
