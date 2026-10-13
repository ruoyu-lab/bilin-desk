#include "incoming_chat.h"

#include "conversations.h"
#include "service_client.h"

#include <QSet>

#include <algorithm>

namespace protocol = rc::protocol;

IncomingChat::IncomingChat(ServiceClient &service, Conversations &conversations)
    : service_(service)
    , conversations_(conversations)
{
    connect(&service_, &ServiceClient::eventReceived, this, &IncomingChat::serviceEvent);
}

void IncomingChat::setViewers(const QList<protocol::SharePeer> &viewers)
{
    viewers_ = viewers;
    for (const protocol::SharePeer &viewer : std::as_const(viewers_))
        connect(conversations_.get(viewer.deviceId), &Conversation::unreadChanged, this, &IncomingChat::updateUnread,
                Qt::UniqueConnection);
    updateUnread();
}

Conversation *IncomingChat::conversation(int id) const
{
    const protocol::SharePeer *peer = viewer(id);
    return peer ? conversations_.get(peer->deviceId) : nullptr;
}

QString IncomingChat::send(int id, const QString &text)
{
    if (service_.state() != ServiceClient::State::Ready)
        return tr("后台服务没有在运行");
    const protocol::SharePeer *peer = viewer(id);
    if (!peer)
        return tr("对方已经断开了");
    const int ref = conversations_.get(peer->deviceId)->addOutgoing(text);
    service_.send(protocol::SendChat,
                  {{QStringLiteral("id"), id}, {QStringLiteral("ref"), ref}, {QStringLiteral("text"), text}});
    return {};
}

void IncomingChat::serviceEvent(const QString &name, const QJsonObject &arguments)
{
    if (name == protocol::ChatReceived) {
        // 说完刚好走了的，已经不知道是谁了
        if (const protocol::SharePeer *peer = viewer(arguments.value(QLatin1StringView("id")).toInt()))
            conversations_.get(peer->deviceId)->addIncoming(arguments.value(QLatin1StringView("text")).toString());
    } else if (name == protocol::ChatFailed) {
        const QString reason = arguments.value(QLatin1StringView("reason")).toString();
        conversations_.markFailed(arguments.value(QLatin1StringView("ref")).toInt(),
                                  reason == protocol::QueueFull ? tr("网络忙，等一下再发") : tr("对方已经断开了"));
    }
}

const protocol::SharePeer *IncomingChat::viewer(int id) const
{
    const auto found = std::find_if(viewers_.cbegin(), viewers_.cend(),
                                    [id](const protocol::SharePeer &peer) { return peer.id == id; });
    return found == viewers_.cend() ? nullptr : &*found;
}

// 同一台设备连着两次（重连时前一次还没走）也只算一份
void IncomingChat::updateUnread()
{
    QSet<Conversation *> counted;
    int unread = 0;
    for (const protocol::SharePeer &viewer : std::as_const(viewers_)) {
        Conversation *conversation = conversations_.get(viewer.deviceId);
        if (!counted.contains(conversation)) {
            counted.insert(conversation);
            unread += conversation->unread();
        }
    }
    if (unread == unread_)
        return;
    unread_ = unread;
    emit unreadChanged();
}
