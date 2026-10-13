#include "conversations.h"

#include "conversation.h"

Conversations::Conversations(QString directory, QObject *parent)
    : QObject(parent)
    , history_(std::move(directory))
{
}

Conversation *Conversations::get(const QString &peer)
{
    Conversation *&conversation = conversations_[peer];
    if (!conversation)
        conversation = new Conversation(history_, peer, this);
    return conversation;
}

void Conversations::markFailed(int id, const QString &reason)
{
    for (Conversation *conversation : std::as_const(conversations_)) {
        if (conversation->markFailed(id, reason))
            return;
    }
}
