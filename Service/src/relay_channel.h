#pragma once

#include <rcsdk/host_transport.hpp>

#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QWebSocket>

#include <functional>

// 网页版（docs/web.md）：分享会话经服务器上的转发进程（Server/src/relay/server.ts）连浏览器的通道，
// SDK 的宿主通道（RCS_TRANSPORT_HOST，数据报）。到转发进程一条 WebSocket，一条二进制消息是 8 字节的路由号（大端，
// 转发进程给每个浏览器的编号，就是 SDK 的 route）加一个数据报；空的数据报：收到是那个浏览器走了，发出去是不要它了。
// SDK 在它的线程上发（send）：排起来交给这个对象的线程写出去，排得太多（对面收不过来）时不收，SDK 当作丢了、重传、
// 降码率。收到的直接交给分享会话（SDK 收的可以在任何线程）
class RelayChannel : public QObject, public rcsdk::HostTransport
{
    Q_OBJECT

public:
    // 一个数据报最大多少：WebSocket 上可以大，消息条数少很多
    static constexpr std::uint32_t MaxDatagram = 16384;

    // 服务器长连接的地址（ws(s)://主机/ws）换成转发进程的（ws(s)://主机/relay）
    static QUrl urlFor(const QUrl &serverUrl);

    explicit RelayChannel(QUrl url, QObject *parent = nullptr);
    ~RelayChannel() override;

    // 收到的交给这个分享会话；头一回见到的路由号先告诉 newRoute（在这个对象的线程上）
    void attach(rcs_session session, std::function<void(quint64 route)> newRoute);
    // 凭一次性的通行证进本机的房间（之前的连接断开）
    void open(const QString &ticket);
    // 连着或者正在连
    bool isOpen() const;
    bool isConnected() const;

    bool send(uint64_t route, const uint8_t *data, uint32_t size) override;

signals:
    // 进了房间（浏览器的握手有人接了）；没进成、断了（通行证不对、转发进程没在运行、网络断了）
    void opened();
    void lost();

private:
    void flush();
    void received(const QByteArray &message);
    // 到转发进程的连接断了（或者换了一条）：这条连接上的浏览器都算走了
    void closeRoutes();

    const QUrl url_;
    QWebSocket socket_;
    rcs_session session_ = nullptr;
    std::function<void(quint64)> newRoute_;
    QSet<quint64> routes_; // 这条连接上来过的浏览器
    bool reopening_ = false;

    // SDK 的线程交来的，等这个对象的线程写出去
    QMutex mutex_;
    QList<QByteArray> queue_;
    qsizetype queued_ = 0;
    bool flushPosted_ = false;
};
