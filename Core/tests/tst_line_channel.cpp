#include <rc/ipc/endpoint.h>
#include <rc/line_channel.h>

#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

// 一对连好的本机连接：server 端是裸的 QLocalSocket（方便发任意字节），client 端是 LineChannel
class TestLineChannel : public QObject
{
    Q_OBJECT

private:
    QLocalServer server_;
    QLocalSocket *peer_ = nullptr;
    rc::LineChannel *channel_ = nullptr;

private slots:
    void init()
    {
        const QString name = rc::ipc::userScopedName(QStringLiteral("rc-test-") + QUuid::createUuid().toString(QUuid::Id128));
        QVERIFY(server_.listen(name));
        auto *socket = new QLocalSocket;
        channel_ = new rc::LineChannel(socket);
        socket->connectToServer(name);
        QVERIFY(server_.waitForNewConnection(3000));
        peer_ = server_.nextPendingConnection();
        QTRY_VERIFY(channel_->isConnected());
    }

    void cleanup()
    {
        delete channel_;
        channel_ = nullptr;
        delete peer_;
        peer_ = nullptr;
        server_.close();
    }

    void sendsOneLinePerMessage()
    {
        channel_->send({{QStringLiteral("command"), QStringLiteral("quit")}});
        channel_->send({{QStringLiteral("text"), QStringLiteral("第一行\n第二行")}});
        QTRY_VERIFY(peer_->bytesAvailable() > 0 && peer_->canReadLine());
        QTRY_COMPARE(peer_->peek(1 << 16).count('\n'), 2);
        QCOMPARE(peer_->readLine(), QByteArray("{\"command\":\"quit\"}\n"));
        QVERIFY(peer_->readLine().endsWith("\"}\n"));
    }

    void receivesMessagesSplitAcrossWrites()
    {
        QSignalSpy spy(channel_, &rc::LineChannel::received);
        peer_->write("{\"topic\":\"dev");
        peer_->flush();
        QTest::qWait(50);
        QCOMPARE(spy.count(), 0);
        peer_->write("ice\"}\n{\"topic\":\"service\"}\n");
        peer_->flush();
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toJsonObject().value(QLatin1StringView("topic")).toString(), QStringLiteral("device"));
        QCOMPARE(spy.at(1).at(0).toJsonObject().value(QLatin1StringView("topic")).toString(), QStringLiteral("service"));
    }

    void disconnectsOnSomethingElse()
    {
        QSignalSpy disconnected(channel_, &rc::LineChannel::disconnected);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("不是 JSON 对象")));
        peer_->write("[1, 2]\n");
        peer_->flush();
        QTRY_COMPARE(disconnected.count(), 1);
    }

    // 对面没在监听：算"没连上"，不算"断了"。macOS 和 Windows 上 QLocalSocket 报错和改状态的先后不一样，两边都要对
    void connectFailedWhenNobodyListens()
    {
        auto *socket = new QLocalSocket;
        rc::LineChannel lonely(socket);
        QSignalSpy failed(&lonely, &rc::LineChannel::connectFailed);
        QSignalSpy disconnected(&lonely, &rc::LineChannel::disconnected);
        socket->connectToServer(rc::ipc::userScopedName(QStringLiteral("rc-test-nobody-")
                                                                 + QUuid::createUuid().toString(QUuid::Id128)));
        QTRY_COMPARE(failed.count(), 1);
        QTest::qWait(50);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(disconnected.count(), 0);
    }

    void disconnectedWhenPeerGoesAway()
    {
        QSignalSpy failed(channel_, &rc::LineChannel::connectFailed);
        QSignalSpy disconnected(channel_, &rc::LineChannel::disconnected);
        peer_->abort();
        QTRY_COMPARE(disconnected.count(), 1);
        QTest::qWait(50);
        QCOMPARE(disconnected.count(), 1);
        QCOMPARE(failed.count(), 0);
    }

    // 局域网信令：TCP 上一样
    void worksOverTcp()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        auto *socket = new QTcpSocket;
        rc::LineChannel client(socket);
        socket->connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(server.waitForNewConnection(3000));
        rc::LineChannel peer(server.nextPendingConnection());
        QTRY_VERIFY(client.isConnected());

        QSignalSpy received(&peer, &rc::LineChannel::received);
        client.send({{QStringLiteral("type"), QStringLiteral("signal")}});
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(received.at(0).at(0).toJsonObject().value(QLatin1StringView("type")).toString(), QStringLiteral("signal"));

        QSignalSpy disconnected(&peer, &rc::LineChannel::disconnected);
        client.close();
        QTRY_COMPARE(disconnected.count(), 1);
    }

    void connectFailedWhenNobodyListensOnTcp()
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost));
        const quint16 port = probe.serverPort();
        probe.close();
        auto *socket = new QTcpSocket;
        rc::LineChannel lonely(socket);
        QSignalSpy failed(&lonely, &rc::LineChannel::connectFailed);
        socket->connectToHost(QHostAddress::LocalHost, port);
        QTRY_COMPARE(failed.count(), 1);
    }

    void disconnectsOnEndlessLine()
    {
        QSignalSpy disconnected(channel_, &rc::LineChannel::disconnected);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("超过")));
        peer_->write(QByteArray(rc::LineChannel::MaxLineBytes + 1024, 'x'));
        QTRY_COMPARE_WITH_TIMEOUT(disconnected.count(), 1, 10000);
    }
};

QTEST_GUILESS_MAIN(TestLineChannel)
#include "tst_line_channel.moc"
