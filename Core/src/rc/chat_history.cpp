#include "rc/chat_history.h"

#include "rc/json_file.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace rc {

ChatHistory::ChatHistory(QString directory)
    : directory_(std::move(directory))
{
}

QList<ChatMessage> ChatHistory::load(const QString &peer) const
{
    const QJsonArray array = readJsonFile(path(peer)).value(QLatin1StringView("messages")).toArray();
    QList<ChatMessage> messages;
    for (qsizetype i = std::max<qsizetype>(0, array.size() - Limit); i < array.size(); ++i) {
        const QJsonObject json = array.at(i).toObject();
        messages.append({json.value(QLatin1StringView("incoming")).toBool(),
                         json.value(QLatin1StringView("text")).toString(),
                         json.value(QLatin1StringView("time")).toInteger(),
                         json.value(QLatin1StringView("error")).toString()});
    }
    return messages;
}

bool ChatHistory::save(const QString &peer, const QList<ChatMessage> &messages) const
{
    QJsonArray array;
    for (qsizetype i = std::max<qsizetype>(0, messages.size() - Limit); i < messages.size(); ++i) {
        const ChatMessage &message = messages.at(i);
        QJsonObject json{
            {QStringLiteral("incoming"), message.incoming},
            {QStringLiteral("text"), message.text},
            {QStringLiteral("time"), message.time},
        };
        if (!message.error.isEmpty())
            json.insert(QStringLiteral("error"), message.error);
        array.append(json);
    }
    if (!QDir().mkpath(directory_)) {
        qWarning("建不了对话记录的目录 %s", qUtf8Printable(directory_));
        return false;
    }
    return writeJsonFile(path(peer), {{QStringLiteral("messages"), array}});
}

// 文件名里不能有冒号（Windows）："ip:地址" 换成 "ip-地址"，IPv6 地址里的冒号也一样（地址里本来没有"-"，不会撞）
QString ChatHistory::path(const QString &peer) const
{
    return directory_ + QLatin1Char('/') + QString(peer).replace(QLatin1Char(':'), QLatin1Char('-'))
         + QStringLiteral(".json");
}

} // namespace rc
