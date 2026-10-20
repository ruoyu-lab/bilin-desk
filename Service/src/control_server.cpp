#include "control_server.h"

#include "client_rights.h"
#include "device_store.h"

#include <rc/line_channel.h>

#include <QLocalSocket>

namespace protocol = rc::protocol;

ControlServer::ControlServer(DeviceStore &store, protocol::ServiceInfo info, Clients clients, QObject *parent)
    : QObject(parent)
    , store_(store)
    , info_(std::move(info))
    , clients_(clients)
{
    server_.setSocketOptions(clients_ == Clients::CurrentUser ? QLocalServer::UserAccessOption
                                                              : QLocalServer::WorldAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, &ControlServer::accept);
    connect(&store_, &DeviceStore::changed, this, [this] { broadcast(deviceMessage()); });
}

bool ControlServer::listen(const QString &endpoint)
{
    QLocalServer::removeServer(endpoint);
    if (server_.listen(endpoint))
        return true;
    qCritical("本机连接开不起来（%s）：%s", qUtf8Printable(endpoint), qUtf8Printable(server_.errorString()));
    return false;
}

void ControlServer::accept()
{
    while (QLocalSocket *socket = server_.nextPendingConnection()) {
        const qintptr descriptor = socket->socketDescriptor();
        if (clients_ == Clients::ConsoleUsers && !client_rights::admit(descriptor)) {
            qWarning("不是控制台前的用户、也不是管理员的程序连上来，断开");
            socket->abort();
            socket->deleteLater();
            continue;
        }
        auto *channel = new rc::LineChannel(socket, this);
        channels_.insert(descriptor, channel);
        connect(channel, &rc::LineChannel::received, this,
                [this, descriptor](const QJsonObject &message) { handle(descriptor, message); });
        connect(channel, &rc::LineChannel::disconnected, this, [this, channel, descriptor] {
            channels_.remove(descriptor);
            for (const QString &session : signalSessions_.take(descriptor))
                emit signalEnded(session);
            channel->deleteLater();
            emit clientsChanged();
        });
        channel->send(protocol::topicMessage(protocol::ServiceTopic, info_.toJson()));
        channel->send(deviceMessage());
        for (auto topic = topics_.cbegin(); topic != topics_.cend(); ++topic)
            channel->send(protocol::topicMessage(topic.key(), topic.value()));
        emit clientsChanged();
    }
}

void ControlServer::publish(QLatin1StringView topic, const QJsonObject &value)
{
    topics_.insert(topic, value);
    broadcast(protocol::topicMessage(topic, value));
}

void ControlServer::handle(qintptr socket, const QJsonObject &message)
{
    const QString command = message.value(QLatin1StringView("command")).toString();
    if (command == protocol::SetDeviceName)
        store_.setDeviceName(message.value(QLatin1StringView("name")).toString());
    else if (command == protocol::SetAcceptIncoming)
        store_.setAcceptIncoming(message.value(QLatin1StringView("value")).toBool());
    else if (command == protocol::SetLanDiscovery)
        store_.setLanDiscovery(message.value(QLatin1StringView("value")).toBool());
    else if (command == protocol::SetUnattended)
        store_.setUnattended(message.value(QLatin1StringView("value")).toBool());
    else if (command == protocol::RegeneratePassword)
        store_.regeneratePassword();
    else if (command == protocol::Quit)
        emit quitRequested();
    else if (command == protocol::AppQuitting)
        emit appQuittingRequested();
    else if (command == protocol::Signal)
        requestSignal(socket, message);
    else if (command == protocol::EndSignal)
        endSignal(socket, message.value(QLatin1StringView("session")).toString());
    else if (command == protocol::Admit)
        emit admitRequested(message.value(QLatin1StringView("id")).toInt(),
                            message.value(QLatin1StringView("control")).toBool());
    else if (command == protocol::Refuse)
        emit refuseRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::Disconnect)
        emit disconnectRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::SetControl)
        emit controlRequested(message.value(QLatin1StringView("id")).toInt(),
                              message.value(QLatin1StringView("value")).toBool());
    else if (command == protocol::SetClipboard)
        emit clipboardRequested(message.value(QLatin1StringView("value")).toBool());
    else if (command == protocol::SetWallpaper)
        emit wallpaperReceived(QByteArray::fromBase64(message.value(QLatin1StringView("jpeg")).toString().toLatin1()));
    else if (command == protocol::RequestPermission)
        emit permissionRequested(message.value(QLatin1StringView("permission")).toString());
    else if (command == protocol::RestartShare)
        emit restartShareRequested();
    else if (command == protocol::SendChat)
        emit chatRequested(message.value(QLatin1StringView("id")).toInt(),
                           message.value(QLatin1StringView("ref")).toInt(),
                           message.value(QLatin1StringView("text")).toString());
    else if (command == protocol::SendCode)
        emit sendCodeRequested(message.value(QLatin1StringView("email")).toString(),
                               message.value(QLatin1StringView("purpose")).toString());
    else if (command == protocol::RegisterAccount)
        emit registerRequested(message.value(QLatin1StringView("email")).toString(),
                               message.value(QLatin1StringView("password")).toString(),
                               message.value(QLatin1StringView("nickname")).toString(),
                               message.value(QLatin1StringView("code")).toString());
    else if (command == protocol::SignIn)
        emit signInRequested(message.value(QLatin1StringView("email")).toString(),
                             message.value(QLatin1StringView("password")).toString());
    else if (command == protocol::ResetPassword)
        emit resetPasswordRequested(message.value(QLatin1StringView("email")).toString(),
                                    message.value(QLatin1StringView("code")).toString(),
                                    message.value(QLatin1StringView("password")).toString());
    else if (command == protocol::SignOut)
        emit signOutRequested();
    else if (command == protocol::RemoveDevice)
        emit removeDeviceRequested(message.value(QLatin1StringView("deviceId")).toString());
    else if (command == protocol::Wake)
        emit wakeDeviceRequested(message.value(QLatin1StringView("deviceId")).toString());
    else if (command == protocol::InstallUpdate)
        emit installUpdateRequested();
    else if (command == protocol::CheckUpdate)
        emit checkUpdateRequested();
    else if (command == protocol::ClearHistory)
        emit clearHistoryRequested();
    else if (command == protocol::SetNickname)
        emit nicknameRequested(message.value(QLatin1StringView("nickname")).toString());
    else if (command == protocol::ChangePassword)
        emit changePasswordRequested(message.value(QLatin1StringView("oldPassword")).toString(),
                                     message.value(QLatin1StringView("newPassword")).toString());
    else if (command == protocol::DeleteAccount)
        emit deleteAccountRequested(message.value(QLatin1StringView("password")).toString());
    else if (command == protocol::SendFiles)
        sendFiles(socket, message);
    else if (command == protocol::AcceptTransfer)
        acceptTransfer(socket, message);
    else if (command == protocol::RejectTransfer)
        emit rejectTransferRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::CancelTransfer)
        emit cancelTransferRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::RetryTransfer)
        emit retryTransferRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::RemoveTransfer)
        emit removeTransferRequested(message.value(QLatin1StringView("id")).toInt());
    else if (command == protocol::FeedbackLogs)
        emit feedbackLogsRequested(socket);
    else if (command == protocol::SendFeedback)
        emit feedbackRequested(socket, message.value(QLatin1StringView("text")).toString(),
                               message.value(QLatin1StringView("contact")).toString(),
                               message.value(QLatin1StringView("logs")).toArray());
    else
        qWarning("不认识的命令：%s", qUtf8Printable(command));
}

void ControlServer::sendFiles(qintptr socket, const QJsonObject &message)
{
    const QStringList paths = message.value(QLatin1StringView("paths")).toVariant().toStringList();
    if (clients_ == Clients::ConsoleUsers && !client_rights::mayRead(socket, paths)) {
        qWarning("界面要发的文件有它的用户自己读不了的，不发");
        return;
    }
    emit sendFilesRequested(message.value(QLatin1StringView("viewer")).toInt(), paths);
}

void ControlServer::acceptTransfer(qintptr socket, const QJsonObject &message)
{
    const QString directory = message.value(QLatin1StringView("directory")).toString();
    if (clients_ == Clients::ConsoleUsers && !client_rights::mayWrite(socket, directory)) {
        qWarning("界面要存到的文件夹它的用户自己写不了，不收");
        return;
    }
    emit acceptTransferRequested(message.value(QLatin1StringView("id")).toInt(), directory);
}

void ControlServer::requestSignal(qintptr socket, const QJsonObject &message)
{
    const std::optional<rc::ConnectionMode> mode = rc::ConnectionMode::read(message);
    const QString session = message.value(QLatin1StringView("session")).toString();
    if (!mode || session.isEmpty()) {
        qWarning("界面发来的信令不对（怎么连、哪次连接）");
        return;
    }
    signalSessions_[socket].insert(session);
    emit signalRequested(message.value(QLatin1StringView("to")).toString(),
                         message.value(QLatin1StringView("address")).toString(), session,
                         message.value(QLatin1StringView("text")).toString(), *mode);
}

void ControlServer::endSignal(qintptr socket, const QString &session)
{
    if (signalSessions_[socket].remove(session))
        emit signalEnded(session);
}

void ControlServer::broadcast(const QJsonObject &message)
{
    for (rc::LineChannel *channel : std::as_const(channels_))
        channel->send(message);
}

void ControlServer::sendEvent(QLatin1StringView event, const QJsonObject &arguments)
{
    broadcast(protocol::eventMessage(event, arguments));
}

void ControlServer::sendEventTo(qintptr socket, QLatin1StringView event, const QJsonObject &arguments)
{
    if (rc::LineChannel *channel = channels_.value(socket))
        channel->send(protocol::eventMessage(event, arguments));
}

QJsonObject ControlServer::deviceMessage() const
{
    return protocol::topicMessage(protocol::DeviceTopic, store_.state().toJson());
}
