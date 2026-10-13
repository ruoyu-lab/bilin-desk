#include "rc/line_channel.h"

#include <QJsonDocument>
#include <QLocalSocket>
#include <QPointer>
#include <QTcpSocket>

namespace rc {

LineChannel::LineChannel(QLocalSocket *socket, QObject *parent)
    : QObject(parent)
{
    attach(socket);
}

LineChannel::LineChannel(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
{
    attach(socket);
}

// 没连上和断了都以"回到未连接"为准：QLocalSocket 在 macOS 上先报错再改状态，在 Windows 上反过来，
// disconnected 信号也只在连上过时才有
template<typename Socket>
void LineChannel::attach(Socket *socket)
{
    socket->setParent(this);
    device_ = socket;
    connected_ = [socket] { return socket->state() == Socket::ConnectedState; };
    abort_ = [socket] { socket->abort(); };
    wasConnected_ = isConnected();
    connect(socket, &Socket::readyRead, this, &LineChannel::read);
    connect(socket, &Socket::connected, this, [this] { wasConnected_ = true; });
    using State = decltype(socket->state());
    connect(socket, &Socket::stateChanged, this, [this](State state) {
        if (state == Socket::UnconnectedState)
            ended();
    });
}

void LineChannel::ended()
{
    if (ended_)
        return;
    ended_ = true;
    if (wasConnected_)
        emit disconnected();
    else
        emit connectFailed();
}

void LineChannel::send(const QJsonObject &message)
{
    if (!isConnected())
        return;
    device_->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void LineChannel::flush(int msecs)
{
    if (isConnected() && device_->bytesToWrite() > 0)
        device_->waitForBytesWritten(msecs);
}

void LineChannel::close()
{
    abort_();
}

void LineChannel::read()
{
    // 收到消息的一方可能当场把这一端关掉、删掉
    const QPointer<LineChannel> self(this);
    while (device_->canReadLine()) {
        const QByteArray line = device_->readLine(MaxLineBytes + 1);
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning("连接收到的不是 JSON 对象（%s），断开", qUtf8Printable(error.errorString()));
            abort_();
            return;
        }
        emit received(document.object());
        if (!self || !isConnected())
            return;
    }
    if (device_->bytesAvailable() > MaxLineBytes) {
        qWarning("连接收到的一行超过 %lld 字节，断开", MaxLineBytes);
        abort_();
    }
}

} // namespace rc
