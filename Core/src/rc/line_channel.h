#pragma once

#include <QJsonObject>
#include <QObject>

#include <functional>

class QIODevice;
class QLocalSocket;
class QTcpSocket;

namespace rc {

// 一行一个 JSON 对象的连接（UTF-8、紧凑格式，"\n" 结尾）：界面程序和后台服务之间的本机连接（QLocalSocket），
// 局域网里两台电脑之间的信令（QTcpSocket）。只用一次：没连上、断了以后换一个新的。
// 对方发来的一行不是 JSON 对象、或者一行超过 MaxLineBytes 还没结束时断开：对面是本程序，
// 收到这样的东西说明对面不对劲，不去猜它想说什么
class LineChannel : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 MaxLineBytes = 1 << 20;

    // 接管 socket（成为它的父对象）；socket 可以已经连上，也可以正在连
    explicit LineChannel(QLocalSocket *socket, QObject *parent = nullptr);
    explicit LineChannel(QTcpSocket *socket, QObject *parent = nullptr);

    bool isConnected() const { return connected_(); }

    void send(const QJsonObject &message);
    // 把还没写出去的都写出去再返回（最多等 msecs）：发完就要退出的时候用
    void flush(int msecs);
    // 断开（随后有 disconnected）
    void close();

signals:
    void received(const QJsonObject &message);
    // 没连上（对面没在监听、拒绝了、找不到）
    void connectFailed();
    // 连上过，后来断了
    void disconnected();

private:
    template<typename Socket>
    void attach(Socket *socket);
    void read();
    void ended();

    QIODevice *device_ = nullptr;
    std::function<bool()> connected_;
    std::function<void()> abort_;
    bool wasConnected_ = false;
    bool ended_ = false;
};

} // namespace rc
