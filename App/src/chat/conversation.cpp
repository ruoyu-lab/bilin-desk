#include "conversation.h"

#include <QDateTime>
#include <QVariantMap>

#include <algorithm>

namespace {

// 每一句的编号：IncomingChat 只凭它找到没发出去的那一句，所以所有对话共用一个
int lastId = 0;

} // namespace

Conversation::Conversation(const rc::ChatHistory &history, QString peer, QObject *parent)
    : QObject(parent)
    , history_(history)
    , peer_(std::move(peer))
{
    for (rc::ChatMessage &message : history_.load(peer_))
        entries_.append({++lastId, std::move(message)});
}

QVariantList Conversation::messages() const
{
    QVariantList list;
    list.reserve(entries_.size());
    for (const Entry &entry : entries_) {
        list.append(QVariantMap{
            {QStringLiteral("incoming"), entry.message.incoming},
            {QStringLiteral("text"), entry.message.text},
            {QStringLiteral("error"), entry.message.error},
        });
    }
    return list;
}

void Conversation::addIncoming(const QString &text)
{
    add({true, text, QDateTime::currentMSecsSinceEpoch(), {}});
    ++unread_;
    emit unreadChanged();
}

int Conversation::addOutgoing(const QString &text)
{
    return add({false, text, QDateTime::currentMSecsSinceEpoch(), {}});
}

bool Conversation::markFailed(int id, const QString &reason)
{
    const auto entry = std::find_if(entries_.begin(), entries_.end(), [id](const Entry &e) { return e.id == id; });
    if (entry == entries_.end())
        return false;
    entry->message.error = reason;
    emit messagesChanged();
    store();
    return true;
}

void Conversation::markRead()
{
    if (unread_ == 0)
        return;
    unread_ = 0;
    emit unreadChanged();
}

int Conversation::add(rc::ChatMessage message)
{
    entries_.append({++lastId, std::move(message)});
    if (entries_.size() > rc::ChatHistory::Limit)
        entries_.removeFirst();
    emit messagesChanged();
    store();
    return entries_.last().id;
}

void Conversation::store()
{
    QList<rc::ChatMessage> messages;
    messages.reserve(entries_.size());
    for (const Entry &entry : std::as_const(entries_))
        messages.append(entry.message);
    const QString error = history_.save(peer_, messages) ? QString() : tr("对话记录没存下来，看看磁盘是不是满了");
    if (error == storeError_)
        return;
    storeError_ = error;
    emit storeErrorChanged();
}
