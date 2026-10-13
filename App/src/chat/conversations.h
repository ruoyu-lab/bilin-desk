#pragma once

#include <rc/chat_history.h>

#include <QHash>
#include <QObject>
#include <QString>

class Conversation;

// 本机和各台设备的对话：每台设备一个 Conversation，第一次用到时从对话记录里读出来，之后一直留着
// （去连它、它连本机都是这一个）。记录在 directory 下（rc/chat_history.h）
class Conversations : public QObject
{
    Q_OBJECT

public:
    Conversations(QString directory, QObject *parent);

    // peer：对方，和设备列表的 key 一样：设备 ID（9 位数字），或者 "ip:地址"
    Conversation *get(const QString &peer);
    // 自己说的那一句（Conversation::addOutgoing 给的编号）没发出去
    void markFailed(int id, const QString &reason);

private:
    const rc::ChatHistory history_;
    QHash<QString, Conversation *> conversations_;
};
