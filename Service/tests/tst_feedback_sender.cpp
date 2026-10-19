#include "device_store.h"
#include "feedback_sender.h"
#include "server_link.h"
#include "updater.h"
#include "test_keys.h"

#include <rc/protocol.h>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

namespace protocol = rc::protocol;

// 假的服务器：收下一个 HTTP 请求（等到 Content-Length 那么多），记下路径和请求体，回 status
class FakeServer : public QObject
{
public:
    int status = 200;
    QByteArray requestLine;
    QByteArray body;

    FakeServer()
    {
        server_.listen(QHostAddress::LocalHost);
        connect(&server_, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = server_.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                buffer_ += socket->readAll();
                const qsizetype end = buffer_.indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                const QByteArray head = buffer_.left(end);
                const qsizetype at = head.toLower().indexOf("content-length:");
                const qsizetype length = head.mid(at + 15, head.indexOf("\r\n", at) - at - 15).trimmed().toLongLong();
                if (buffer_.size() - end - 4 < length)
                    return;
                requestLine = head.left(head.indexOf("\r\n"));
                body = buffer_.mid(end + 4, length);
                const QByteArray answer = R"({"ok":true})";
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\n"
                              "Content-Length: " + QByteArray::number(answer.size()) + "\r\nConnection: close\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server_.serverPort())); }

private:
    QTcpServer server_;
    QByteArray buffer_;
};

class TestFeedbackSender : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir directory_;

    // 交给 base，等回答
    QString send(const QUrl &base, const QList<rc::feedback::Log> &logs = {})
    {
        DeviceStore store(directory_.path(), testDeviceKey);
        FeedbackSender sender(store, base, {});
        std::optional<QString> reason;
        sender.send(QStringLiteral("画面是黑的"), QStringLiteral("微信 abc"), logs, [&](const QString &r) { reason = r; });
        [&] { QTRY_VERIFY_WITH_TIMEOUT(reason, 10000); }();
        return reason.value_or(QStringLiteral("没回答"));
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationVersion(QStringLiteral("0.1.5-beta"));
    }

    // 带着设备 ID、令牌、版本、系统交给 POST /feedback
    void postsWithDeviceAndVersion()
    {
        FakeServer server;
        QCOMPARE(send(server.url(), {{QStringLiteral("app.log"), QStringLiteral("连接：直连\n")}}), QString());
        QCOMPARE(server.requestLine, QByteArray("POST /feedback HTTP/1.1"));
        const QJsonObject body = QJsonDocument::fromJson(server.body).object();
        DeviceStore store(directory_.path(), testDeviceKey);
        QCOMPARE(body.value(QLatin1StringView("deviceId")).toString(), store.state().deviceId);
        QCOMPARE(body.value(QLatin1StringView("token")).toString(), store.deviceToken());
        QCOMPARE(body.value(QLatin1StringView("version")).toString(), QStringLiteral("0.1.5-beta"));
        QCOMPARE(body.value(QLatin1StringView("system")).toString(), rc::feedback::systemName());
        QCOMPARE(body.value(QLatin1StringView("text")).toString(), QStringLiteral("画面是黑的"));
        QCOMPARE(body.value(QLatin1StringView("contact")).toString(), QStringLiteral("微信 abc"));
        QCOMPARE(body.value(QLatin1StringView("logs")).toArray(),
                 (QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("app.log")},
                                         {QStringLiteral("text"), QStringLiteral("连接：直连\n")}}}));
    }

    void reportsWhyItFailed()
    {
        FakeServer server;
        server.status = 429;
        QCOMPARE(send(server.url()), QString(protocol::TooMany));
        server.status = 400;
        QCOMPARE(send(server.url()), QString(protocol::Failed));
        // 没有在听的端口
        QTcpServer closed;
        closed.listen(QHostAddress::LocalHost);
        const QUrl nobody(QStringLiteral("http://127.0.0.1:%1").arg(closed.serverPort()));
        closed.close();
        QCOMPARE(send(nobody), QString(protocol::NoServer));
    }

    void readsItsOwnLogs()
    {
        QFile log(directory_.filePath(QStringLiteral("service.log")));
        QVERIFY(log.open(QIODevice::WriteOnly));
        log.write("后台服务启动\n");
        log.close();
        DeviceStore store(directory_.path(), testDeviceKey);
        FeedbackSender sender(store, {}, {log.fileName(), directory_.filePath(QStringLiteral("service.log.previous"))});
        QCOMPARE(sender.logs(), (QList<rc::feedback::Log>{{QStringLiteral("service.log"), QStringLiteral("后台服务启动\n")}}));
    }

    // 真的服务器：先登记本机（长连接），再提交一条带日志的反馈，服务器收下。
    // 环境变量 RC_FEEDBACK_SERVER 是服务器长连接的地址（比如开发用的虚拟机 ws://localhost:8800/ws）；没设时跳过
    void sendsToARealServer()
    {
        const QString url = qEnvironmentVariable("RC_FEEDBACK_SERVER");
        if (url.isEmpty())
            QSKIP("没设 RC_FEEDBACK_SERVER：不连真的服务器");
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        store.setDeviceName(QStringLiteral("反馈测试"));
        ServerLink link(store, QUrl(url), {});
        link.start();
        QTRY_COMPARE_WITH_TIMEOUT(link.state(), ServerLink::State::Online, 10000);
        FeedbackSender sender(store, HttpUpdateSource::baseFor(QUrl(url)), {});
        std::optional<QString> reason;
        sender.send(QStringLiteral("端到端测试：tst_feedback_sender 提交的"), {},
                    {{QStringLiteral("service.log"), QStringLiteral("后台服务启动\n连接 482915736：直连\n")}},
                    [&](const QString &r) { reason = r; });
        QTRY_VERIFY_WITH_TIMEOUT(reason, 10000);
        QCOMPARE(*reason, QString());
    }
};

QTEST_GUILESS_MAIN(TestFeedbackSender)
#include "tst_feedback_sender.moc"
