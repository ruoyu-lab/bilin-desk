#include "feedback_sender.h"

#include "device_store.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace protocol = rc::protocol;

namespace {

// 日志有几 MB，慢的网络上传得一会儿
constexpr int TimeoutMs = 120 * 1000;

} // namespace

FeedbackSender::FeedbackSender(DeviceStore &store, QUrl base, QStringList logFiles, QObject *parent)
    : QObject(parent)
    , store_(store)
    , base_(std::move(base))
    , logFiles_(std::move(logFiles))
{
}

void FeedbackSender::send(const QString &text, const QString &contact, const QList<rc::feedback::Log> &logs,
                          std::function<void(QString)> done)
{
    QJsonArray logsJson;
    for (const rc::feedback::Log &log : logs)
        logsJson.append(QJsonObject{{QStringLiteral("name"), log.name}, {QStringLiteral("text"), log.text}});
    const QJsonObject body{
        {QStringLiteral("deviceId"), store_.state().deviceId},
        {QStringLiteral("token"), store_.deviceToken()},
        {QStringLiteral("version"), QCoreApplication::applicationVersion()},
        {QStringLiteral("system"), rc::feedback::systemName()},
        {QStringLiteral("contact"), contact},
        {QStringLiteral("text"), text},
        {QStringLiteral("logs"), logsJson},
    };

    QUrl url = base_;
    url.setPath(QStringLiteral("/feedback"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(TimeoutMs);
    QNetworkReply *reply = network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200) {
            qInfo("反馈交给服务器了");
            done({});
        } else if (status == 429) {
            done(protocol::TooMany);
        } else if (status == 0) {
            qWarning("反馈没交上，连不上服务器：%s", qUtf8Printable(reply->errorString()));
            done(protocol::NoServer);
        } else {
            qWarning("反馈没交上，服务器回答 %d：%s", status, reply->readAll().left(200).constData());
            done(protocol::Failed);
        }
    });
}
