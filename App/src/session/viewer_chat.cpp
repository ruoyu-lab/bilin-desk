#include "viewer_chat.h"

#include "viewer_session.h"

#include <rc/chat_protocol.h>
#include <rc/peer_protocol.h>

ViewerChat::ViewerChat(ViewerSession &session, Conversation &conversation)
    : session_(session)
    , conversation_(conversation)
{
}

QString ViewerChat::send(const QString &text)
{
    const QString status = session_.status();
    if (status == u"reconnecting")
        return tr("路断了，正在重连");
    if (status == u"failed")
        return tr("连接已经断开了");
    if (status != u"connected")
        return tr("还没连上");
    if (!attended_)
        return tr("对方电脑上没打开本程序，看不到消息");
    switch (session_.sendData(rc::peer::ChatChannel, rc::chat::text(text))) {
    case rc::peer::SendResult::Sent:
        conversation_.addOutgoing(text);
        return {};
    case rc::peer::SendResult::QueueFull:
        return tr("网络忙，等一下再发");
    case rc::peer::SendResult::NotConnected:
        return tr("连接已经断开了");
    }
    Q_UNREACHABLE_RETURN({});
}

void ViewerChat::receive(const QByteArray &data)
{
    const auto message = rc::chat::parse(data);
    if (!message) {
        qWarning("对方发来的对话消息不对（%lld 字节）", qlonglong(data.size()));
        return;
    }
    if (const auto *text = std::get_if<rc::chat::Text>(&*message)) {
        conversation_.addIncoming(text->text);
        return;
    }
    attended_ = std::get<rc::chat::Presence>(*message).attended;
    qInfo("对方电脑上%s界面程序", attended_ ? "开着" : "没开着");
}
