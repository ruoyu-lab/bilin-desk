#include "single_instance.h"

#include <rc/ipc/endpoint.h>

#include <QLocalSocket>

namespace {

constexpr int ConnectTimeoutMs = 1000;
constexpr char ShowMessage[] = "show\n";

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    // 按用户区分：同一台机器上的另一个账户可以有自己的一个
    name_ = rc::ipc::userScopedName(QStringLiteral("com.bilindesk.bilin"));
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = server_.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (socket->readAll().contains(ShowMessage))
                    emit activated();
            });
        }
    });
}

bool SingleInstance::forwardToRunning()
{
    QLocalSocket socket;
    socket.connectToServer(name_);
    if (socket.waitForConnected(ConnectTimeoutMs)) {
        socket.write(ShowMessage);
        socket.waitForBytesWritten(ConnectTimeoutMs);
        socket.disconnectFromServer();
        return true;
    }
    // 上一次异常退出时留下的套接字文件（macOS）：清掉再监听
    QLocalServer::removeServer(name_);
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_.listen(name_))
        qWarning("单实例监听失败: %s", qUtf8Printable(server_.errorString()));
    return false;
}
