#pragma once

#include "conversation.h"

#include <rc/protocol.h>

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class Conversations;
class ServiceClient;

// 别人连本机时的对话（IncomingConnections 的 chat）：每个连着本机的人一份，就是和他那台设备的对话
// （Conversation，不管谁连谁都是同一份）。经后台服务收发（rc/protocol.h 的 sendChat、chatReceived、chatFailed）
class IncomingChat : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("属于 IncomingConnections")

    // 连着本机的人说的、还没看的一共几条
    Q_PROPERTY(int unread READ unread NOTIFY unreadChanged)

public:
    IncomingChat(ServiceClient &service, Conversations &conversations);

    // 现在连着本机的人（share 主题的 viewers）
    void setViewers(const QList<rc::protocol::SharePeer> &viewers);
    int unread() const { return unread_; }

    // 和连着本机的人 id 的对话；不是连着的人时为空
    Q_INVOKABLE Conversation *conversation(int id) const;
    // 对 id 说一句：返回空字符串表示交给后台服务了，否则是没发出去的原因。
    // 之后才知道没发出去的（对方那边排着的太多、刚好断了），在那一句下面写原因
    Q_INVOKABLE QString send(int id, const QString &text);

signals:
    void unreadChanged();

private:
    void serviceEvent(const QString &name, const QJsonObject &arguments);
    const rc::protocol::SharePeer *viewer(int id) const;
    void updateUnread();

    ServiceClient &service_;
    Conversations &conversations_;
    QList<rc::protocol::SharePeer> viewers_;
    int unread_ = 0;
};
