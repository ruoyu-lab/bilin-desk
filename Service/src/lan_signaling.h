#pragma once

#include <rc/lan_protocol.h>

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTcpServer>

class DeviceStore;

namespace rc {
class LineChannel;
}

// 局域网里的信令（rc/lan_protocol.h）：不经服务器，直接连到对方电脑的信令端口。没有外网也能连。
// - 去连别人：每次连接（session）一条 TCP 连接，信令从它发出去，对方的回答、回绝从它回来
// - 别人来连：听着信令端口，记下每次连接是哪条 TCP 连接，本机的回答、回绝从那条发回去
// 连着的时候换了网络还要再发信令，TCP 连接留到这次连接结束（Signaling 说忘掉它，close）
class LanSignaling : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        quint16 listenPort = rc::lan::SignalingPort; // 别人来连本机
        quint16 peerPort = rc::lan::SignalingPort;   // 本机去连别人
    };

    LanSignaling(DeviceStore &store, Options options, QObject *parent = nullptr);

    // 开始听别人来连；端口用不了时 false（日志里有原因），这时只能去连别人
    bool listen();
    quint16 port() const { return server_.serverPort(); }

    // 去连 address 那台电脑：发这次连接的信令（mode 是这次连接怎么连）
    void send(const QString &address, const QString &session, const QString &text, const rc::ConnectionMode &mode);
    // 回答、回绝来连本机的那次连接
    void answer(const QString &session, const QString &text, const rc::ConnectionMode &mode);
    void decline(const QString &session, const QString &reason);
    // 这次连接结束了：关掉它的 TCP 连接
    void close(const QString &session);

signals:
    // address 那台电脑（它说自己是设备 from，叫 name）发来的信令：来连本机的，或者本机去连的那台的回答
    void received(const QString &address, const QString &from, const QString &name, const QString &session,
                  const QString &text, const rc::ConnectionMode &mode);
    // 本机去连的那台回绝了
    void declined(const QString &from, const QString &session, const QString &reason);
    // 连不上本机去连的那台电脑
    void unreachable(const QString &session);

private:
    struct Link
    {
        rc::LineChannel *channel = nullptr;
        QString address;
        QList<QJsonObject> pending; // 连上之前要发的
    };

    void accept();
    void handle(rc::LineChannel *channel, const QString &address, const QJsonObject &message);
    void drop(rc::LineChannel *channel);

    DeviceStore &store_;
    const Options options_;
    QTcpServer server_;
    QHash<QString, Link> outgoing_; // 本机去连别人，按 session
    QHash<QString, Link> incoming_; // 别人来连本机，按 session
};
