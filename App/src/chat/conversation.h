#pragma once

#include <rc/chat_history.h>

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// 和一台设备的对话（Conversations 里每台设备一份）：说过的话、还没看的有几条。不管谁连谁都是这一份；
// 收发在 ViewerChat（去连别人时）和 IncomingChat（别人连本机时）里，这里只记。
// 每一句都存进本机的对话记录（rc/chat_history.h），下次连上还在；只留最近 ChatHistory::Limit 条。
// messages 每一项是 { incoming, text, error（自己说的没发出去时为什么） }
class Conversation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由 Conversations 创建")

    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(int unread READ unread NOTIFY unreadChanged)
    // 对话记录没存下来时的说明（磁盘满了、没有权限）；存下来了是空的
    Q_PROPERTY(QString storeError READ storeError NOTIFY storeErrorChanged)

public:
    // peer：对方，和设备列表的 key 一样（rc/chat_history.h）
    Conversation(const rc::ChatHistory &history, QString peer, QObject *parent);

    QVariantList messages() const;
    int unread() const { return unread_; }
    QString storeError() const { return storeError_; }

    // 对方说的
    void addIncoming(const QString &text);
    // 自己说的（已经交出去了）；返回这一句的编号，这次运行里所有的对话都不重复
    int addOutgoing(const QString &text);
    // 自己说的 id 那一句没发出去；不是这份对话里的返回 false
    bool markFailed(int id, const QString &reason);
    Q_INVOKABLE void markRead();

signals:
    void messagesChanged();
    void unreadChanged();
    void storeErrorChanged();

private:
    struct Entry
    {
        int id = 0;
        rc::ChatMessage message;
    };

    int add(rc::ChatMessage message);
    void store();

    const rc::ChatHistory &history_;
    const QString peer_;
    QList<Entry> entries_;
    int unread_ = 0;
    QString storeError_;
};
