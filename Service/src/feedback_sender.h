#pragma once

#include <rc/feedback_logs.h>

#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>

class DeviceStore;

// 提交反馈（docs/feedback.md）：带着本机的设备 ID 和令牌交给服务器的 POST /feedback，版本和系统由这里带上。
// 后台服务自己的日志也从这里读（logFiles：装好时只有 SYSTEM / root 读得了，界面程序要了给人看）
class FeedbackSender : public QObject
{
    Q_OBJECT

public:
    // base：服务器的地址（https://www.bilindesk.com，HttpUpdateSource::baseFor）
    FeedbackSender(DeviceStore &store, QUrl base, QStringList logFiles, QObject *parent = nullptr);

    QList<rc::feedback::Log> logs() const { return rc::feedback::readLogs(logFiles_); }
    // done(reason)：空的是交上了；不然是 rc/protocol.h 的 FeedbackFailed 的 reason
    void send(const QString &text, const QString &contact, const QList<rc::feedback::Log> &logs,
              std::function<void(QString reason)> done);

private:
    DeviceStore &store_;
    const QUrl base_;
    const QStringList logFiles_;
    QNetworkAccessManager network_;
};
