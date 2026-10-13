#pragma once

#include "conversation.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class ViewerSession;

// 去连别人的会话里的对话（ViewerSession 的 chat）：在对话通道上收发（rc/chat_protocol.h），
// 说过的话记在和这台设备的对话（Conversation）里。连上以后才能说；对方电脑上没打开界面程序时
// （对方说的，presence）没人看，也不让说
class ViewerChat : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("属于 ViewerSession")

    Q_PROPERTY(Conversation *conversation READ conversation CONSTANT)

public:
    ViewerChat(ViewerSession &session, Conversation &conversation);

    Conversation *conversation() const { return &conversation_; }

    // 说一句：返回空字符串表示发出去了，否则是没发出去的原因
    Q_INVOKABLE QString send(const QString &text);
    // 对话通道上对方发来的
    void receive(const QByteArray &data);

private:
    ViewerSession &session_;
    Conversation &conversation_;
    // 对方电脑上开着界面程序（对方连上时会说）
    bool attended_ = true;
};
