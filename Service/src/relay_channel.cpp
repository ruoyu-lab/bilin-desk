#include "relay_channel.h"

#include <QMutexLocker>
#include <QUrlQuery>
#include <QtEndian>

namespace {

constexpr qsizetype RouteBytes = 8;
// 排着等写出去的、已经交给 WebSocket 还没发完的，各最多这么多
constexpr qsizetype MaxQueued = 2 * 1024 * 1024;

} // namespace

QUrl RelayChannel::urlFor(const QUrl &serverUrl)
{
    QUrl url = serverUrl;
    url.setPath(QStringLiteral("/relay"));
    return url;
}

RelayChannel::RelayChannel(QUrl url, QObject *parent)
    : QObject(parent)
    , url_(std::move(url))
{
    connect(&socket_, &QWebSocket::binaryMessageReceived, this, &RelayChannel::received);
    connect(&socket_, &QWebSocket::connected, this, [] { qInfo("网页版：进了转发进程里本机的房间"); });
    connect(&socket_, &QWebSocket::disconnected, this, [this] {
        if (reopening_)
            return;
        qInfo("网页版：和转发进程断开了（%s）", qUtf8Printable(socket_.closeReason()));
        closeRoutes();
    });
}

RelayChannel::~RelayChannel()
{
    socket_.disconnect(this);
    socket_.abort();
}

void RelayChannel::attach(rcs_session session, std::function<void(quint64)> newRoute)
{
    session_ = session;
    newRoute_ = std::move(newRoute);
}

void RelayChannel::open(const QString &ticket)
{
    reopening_ = true;
    socket_.abort();
    reopening_ = false;
    closeRoutes();
    QUrl url = url_;
    url.setQuery(QUrlQuery{{QStringLiteral("ticket"), ticket}});
    socket_.open(url);
}

bool RelayChannel::isOpen() const
{
    return socket_.state() == QAbstractSocket::ConnectingState || socket_.state() == QAbstractSocket::ConnectedState;
}

bool RelayChannel::send(uint64_t route, const uint8_t *data, uint32_t size)
{
    QByteArray message(RouteBytes + qsizetype(size), Qt::Uninitialized);
    qToBigEndian<quint64>(route, message.data());
    std::memcpy(message.data() + RouteBytes, data, size);
    const QMutexLocker locker(&mutex_);
    if (queued_ + message.size() > MaxQueued)
        return false;
    queued_ += message.size();
    queue_.append(std::move(message));
    if (!flushPosted_) {
        flushPosted_ = true;
        QMetaObject::invokeMethod(this, &RelayChannel::flush, Qt::QueuedConnection);
    }
    return true;
}

void RelayChannel::flush()
{
    QList<QByteArray> messages;
    {
        const QMutexLocker locker(&mutex_);
        messages.swap(queue_);
        queued_ = 0;
        flushPosted_ = false;
    }
    if (socket_.state() != QAbstractSocket::ConnectedState)
        return;
    for (const QByteArray &message : std::as_const(messages)) {
        // 对面收不过来，WebSocket 里还积着：丢掉，SDK 自己重传、降码率
        if (socket_.bytesToWrite() > MaxQueued)
            return;
        socket_.sendBinaryMessage(message);
    }
}

void RelayChannel::received(const QByteArray &message)
{
    if (message.size() < RouteBytes)
        return;
    const quint64 route = qFromBigEndian<quint64>(message.constData());
    if (message.size() == RouteBytes) {
        if (routes_.remove(route))
            rcs_session_transport_closed(session_, route);
        return;
    }
    if (!routes_.contains(route)) {
        routes_.insert(route);
        newRoute_(route);
    }
    rcs_session_transport_receive(session_, route, reinterpret_cast<const uint8_t *>(message.constData()) + RouteBytes,
                                  uint32_t(message.size() - RouteBytes));
}

void RelayChannel::closeRoutes()
{
    for (const quint64 route : std::as_const(routes_))
        rcs_session_transport_closed(session_, route);
    routes_.clear();
}
