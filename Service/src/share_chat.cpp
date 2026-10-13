#include "share_chat.h"

#include <rc/chat_protocol.h>

namespace protocol = rc::protocol;

ShareChat::ShareChat(Sender sender, QObject *parent)
    : QObject(parent)
    , sender_(std::move(sender))
{
}

void ShareChat::setAttended(bool attended)
{
    if (attended == attended_)
        return;
    attended_ = attended;
    for (const int id : std::as_const(viewers_))
        tellPresence(id);
}

void ShareChat::setViewers(const QList<protocol::SharePeer> &viewers)
{
    QList<int> ids;
    for (const protocol::SharePeer &viewer : viewers) {
        ids.append(viewer.id);
        if (!viewers_.contains(viewer.id))
            tellPresence(viewer.id);
    }
    viewers_ = std::move(ids);
}

void ShareChat::receive(int id, const QByteArray &data)
{
    const auto message = rc::chat::parse(data);
    if (!message) {
        qWarning("连着本机的人 %d 发来的对话消息不对（%lld 字节）", id, qlonglong(data.size()));
        return;
    }
    const auto *text = std::get_if<rc::chat::Text>(&*message);
    if (!text) {
        qWarning("连着本机的人 %d 发来了只有被控的一方才发的对话消息", id);
        return;
    }
    // 没人看：对方那边收到 presence 以后就不让说了，这是在那之前刚好说的
    if (!attended_) {
        qInfo("连着本机的人 %d 说了一句话，界面程序没开着，没人看", id);
        return;
    }
    emit received(id, text->text);
}

void ShareChat::send(int id, int ref, const QString &text)
{
    switch (sender_(id, rc::chat::text(text))) {
    case rc::peer::SendResult::Sent:
        return;
    case rc::peer::SendResult::QueueFull:
        emit failed(id, ref, protocol::QueueFull);
        return;
    case rc::peer::SendResult::NotConnected:
        emit failed(id, ref, protocol::NotConnected);
        return;
    }
}

void ShareChat::tellPresence(int id)
{
    if (sender_(id, rc::chat::presence(attended_)) != rc::peer::SendResult::Sent)
        qWarning("没能告诉连着本机的人 %d 本机%s界面程序", id, attended_ ? "开着" : "没开着");
}
