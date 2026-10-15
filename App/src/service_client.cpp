#include "service_client.h"

#include <rc/line_channel.h>
#include <rc/programs.h>
#include <rc/protocol.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>

namespace protocol = rc::protocol;

namespace {

// 启动后台服务以后，每隔 RetryMs 连一次，一共 MaxAttempts 次（约 5 秒）
constexpr int RetryMs = 250;
constexpr int MaxAttempts = 20;

// 旁边那一份后台服务：Windows 上和界面程序在同一个目录，macOS 上在 .app 的 Contents/Helpers 里
QString serviceProgram()
{
    return QStandardPaths::findExecutable(rc::program::Service,
                                          {rc::program::helpersDirectory(QCoreApplication::applicationDirPath())});
}

} // namespace

ServiceClient::ServiceClient(Runner runner, const QString &endpoint, QObject *parent)
    : QObject(parent)
    , runner_(runner)
    , endpoint_(endpoint)
{
    retryTimer_.setSingleShot(true);
    retryTimer_.setInterval(RetryMs);
    connect(&retryTimer_, &QTimer::timeout, this, &ServiceClient::connectToService);
}

void ServiceClient::start()
{
    connectToService();
}

void ServiceClient::retry()
{
    attempts_ = 0;
    launched_ = false;
    replaced_ = false;
    setState(State::Connecting);
    connectToService();
}

void ServiceClient::stopService()
{
    stopping_ = true;
    retryTimer_.stop();
    if (!channel_ || runner_ != Runner::Beside)
        return;
    channel_->send(protocol::commandMessage(protocol::Quit));
    channel_->flush(500);
}

void ServiceClient::appQuitting()
{
    if (state_ != State::Ready)
        return;
    channel_->send(protocol::commandMessage(protocol::AppQuitting));
    channel_->flush(500);
}

void ServiceClient::send(QLatin1StringView command, const QJsonObject &arguments)
{
    if (state_ == State::Ready)
        channel_->send(protocol::commandMessage(command, arguments));
}

void ServiceClient::connectToService()
{
    if (stopping_)
        return;
    // 上一次的连接已经断了或者没连上：不再听它的
    if (channel_) {
        channel_->disconnect(this);
        channel_->deleteLater();
    }
    auto *socket = new QLocalSocket;
    channel_ = new rc::LineChannel(socket, this);
    connect(channel_, &rc::LineChannel::connectFailed, this, &ServiceClient::connectionFailed);
    connect(channel_, &rc::LineChannel::disconnected, this, &ServiceClient::disconnected);
    connect(channel_, &rc::LineChannel::received, this, &ServiceClient::received);
    socket->connectToServer(endpoint_);
}

void ServiceClient::connectionFailed()
{
    ++attempts_;
    // 系统服务由系统启动（开机、被看护者重启）：等它
    if (!launched_ && runner_ == Runner::Beside) {
        launched_ = true;
        launch();
    }
    if (attempts_ < MaxAttempts) {
        retryTimer_.start();
        return;
    }
    qWarning("后台服务启动了也连不上");
    setState(State::Unavailable);
}

void ServiceClient::disconnected()
{
    if (stopping_)
        return;
    qInfo("和后台服务的连接断了，重新连");
    setState(State::Connecting);
    retryTimer_.start();
}

void ServiceClient::launch()
{
#if QT_CONFIG(process) // 只有电脑上有（Runner::Beside）：iOS 不能启动别的程序
    const QString program = serviceProgram();
    if (program.isEmpty()) {
        qWarning("程序旁边没有后台服务（%s）", rc::program::Service.data());
        return;
    }
    qInfo("启动后台服务：%s", qUtf8Printable(program));
    if (!QProcess::startDetached(program, {}, QFileInfo(program).absolutePath()))
        qWarning("后台服务启动不了：%s", qUtf8Printable(program));
#endif
}

void ServiceClient::received(const QJsonObject &message)
{
    if (const QString event = message.value(QLatin1StringView("event")).toString(); !event.isEmpty()) {
        emit eventReceived(event, message);
        return;
    }
    const QString topic = message.value(QLatin1StringView("topic")).toString();
    if (topic.isEmpty())
        return;
    const QJsonObject value = message.value(QLatin1StringView("value")).toObject();
    if (topic == protocol::ServiceTopic) {
        checkService(value);
        return;
    }
    topics_.insert(topic, value);
    emit topicChanged(topic);
}

void ServiceClient::checkService(const QJsonObject &json)
{
    if (runner_ != Runner::Beside) {
        attempts_ = 0;
        setState(State::Ready);
        return;
    }
    const protocol::ServiceInfo running = protocol::ServiceInfo::fromJson(json);
    const QFileInfo beside(serviceProgram());
    const bool same = beside.exists()
                      && QFileInfo(running.program).canonicalFilePath() == beside.canonicalFilePath()
                      && running.modified == beside.lastModified().toMSecsSinceEpoch();
    // 换过一次还不一样（比如旁边那一份启动以后又被重新编译了）：先用着，下次打开界面再换
    if (!same && beside.exists() && !replaced_) {
        qInfo("正在运行的后台服务（%s）不是旁边这一份，换掉它", qUtf8Printable(running.program));
        replaced_ = true;
        launched_ = false;
        attempts_ = 0;
        channel_->send(protocol::commandMessage(protocol::Quit));
        return;
    }
    attempts_ = 0;
    launched_ = false;
    replaced_ = false;
    setState(State::Ready);
}

void ServiceClient::setState(State state)
{
    if (state == state_)
        return;
    state_ = state;
    emit stateChanged();
}
