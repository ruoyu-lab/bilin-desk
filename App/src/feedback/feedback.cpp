#include "feedback.h"

#include "service_client.h"

#include <rc/line_channel.h>
#include <rc/protocol.h>

#include <QJSEngine>
#include <QJsonDocument>
#include <QLocale>

namespace protocol = rc::protocol;

namespace {

Feedback *instance = nullptr;

QString sizeText(qsizetype bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString failureMessage(const QString &reason)
{
    if (reason == protocol::TooMany)
        return Feedback::tr("今天提交得太多了，明天再试");
    if (reason == protocol::NoServer)
        return Feedback::tr("连不上服务器，检查一下网络再试");
    return Feedback::tr("没提交上，过一会儿再试");
}

} // namespace

Feedback::Feedback(ServiceClient &service, QStringList logFiles, QObject *parent)
    : QObject(parent)
    , service_(service)
    , logFiles_(std::move(logFiles))
{
    connect(&service_, &ServiceClient::eventReceived, this, [this](const QString &name, const QJsonObject &arguments) {
        if (name == protocol::FeedbackLogs && loadingLogs_) {
            const std::optional<QList<rc::feedback::Log>> logs =
                rc::feedback::unpackLogs(arguments.value(QLatin1StringView("logs")).toArray());
            if (logs)
                logs_.append(*logs);
            loadingLogs_ = false;
            emit logsChanged();
        } else if (name == protocol::FeedbackSent && busy_) {
            setBusy(false);
            emit sent();
        } else if (name == protocol::FeedbackFailed && busy_) {
            setBusy(false);
            emit failed(failureMessage(arguments.value(QLatin1StringView("reason")).toString()));
        }
    });
    // 等回答的时候后台服务断了：回答不会来了
    connect(&service_, &ServiceClient::stateChanged, this, [this] {
        if (service_.state() == ServiceClient::State::Ready)
            return;
        if (loadingLogs_) {
            loadingLogs_ = false;
            emit logsChanged();
        }
        if (busy_) {
            setBusy(false);
            emit failed(tr("后台服务没有在运行"));
        }
    });
}

void Feedback::setInstance(Feedback *feedback)
{
    instance = feedback;
}

Feedback *Feedback::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

QVariantList Feedback::logs() const
{
    QVariantList list;
    for (const rc::feedback::Log &log : logs_)
        list.append(QVariantMap{{QStringLiteral("name"), log.name},
                                {QStringLiteral("size"), sizeText(log.text.toUtf8().size())}});
    return list;
}

QString Feedback::logsSize() const
{
    qsizetype bytes = 0;
    for (const rc::feedback::Log &log : logs_)
        bytes += log.text.toUtf8().size();
    return sizeText(bytes);
}

void Feedback::loadLogs()
{
    logs_ = rc::feedback::readLogs(logFiles_);
    loadingLogs_ = service_.state() == ServiceClient::State::Ready;
    if (loadingLogs_)
        service_.send(protocol::FeedbackLogs);
    emit logsChanged();
}

QString Feedback::logText(int index) const
{
    return index >= 0 && index < logs_.size() ? logs_.at(index).text : QString();
}

void Feedback::send(const QString &text, const QString &contact, bool withLogs)
{
    const QJsonObject arguments{
        {QStringLiteral("text"), text.trimmed()},
        {QStringLiteral("contact"), contact.trimmed()},
        {QStringLiteral("logs"), withLogs ? rc::feedback::packLogs(logs_) : QJsonArray()},
    };
    // 本机连接上一条消息最多 1 MB：日志压缩以后一般只有几百 KB，超了的只能不带日志
    if (QJsonDocument(arguments).toJson(QJsonDocument::Compact).size() >= rc::LineChannel::MaxLineBytes - 1024) {
        emit failed(tr("日志太大了，关掉“附带日志”再提交"));
        return;
    }
    setBusy(true);
    service_.send(protocol::SendFeedback, arguments);
}

void Feedback::setBusy(bool busy)
{
    if (busy == busy_)
        return;
    busy_ = busy;
    emit busyChanged();
}
