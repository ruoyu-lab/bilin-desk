#pragma once

#include <rc/data_channels.h>
#include <rc/protocol.h>

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

// 别人连本机时的对话（对话通道，rc/chat_protocol.h）：连着本机的人说的话交给界面（received），
// 界面上的人说的发给对方（send）。按连着本机的人（share 主题里的 id）分开。
// 没有界面连着后台服务时没人看：告诉连着的人（presence），他们那边就不让说了；这时收到的话丢掉
class ShareChat : public QObject
{
    Q_OBJECT

public:
    // 发给连着本机的人 id 一条（对话通道上）
    using Sender = std::function<rc::peer::SendResult(int id, const QByteArray &data)>;

    explicit ShareChat(Sender sender, QObject *parent = nullptr);

    // 有没有界面连着后台服务
    void setAttended(bool attended);
    // 现在连着本机的人（share 主题的 viewers）：新连进来的，告诉他本机有没有人看
    void setViewers(const QList<rc::protocol::SharePeer> &viewers);
    // 对话通道上，连着本机的人 id 发来的
    void receive(int id, const QByteArray &data);
    // 界面上的人对 id 说一句；ref 是界面给这一句的编号，没发出去时带着它告诉界面
    void send(int id, int ref, const QString &text);

signals:
    void received(int id, const QString &text);
    // reason 见 rc/protocol.h 的 ChatFailed
    void failed(int id, int ref, const QString &reason);

private:
    void tellPresence(int id);

    Sender sender_;
    bool attended_ = false;
    QList<int> viewers_;
};
