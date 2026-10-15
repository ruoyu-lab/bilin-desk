#include <rc/transport.h>

#include <QJsonArray>
#include <QTest>

class TestTransport : public QObject
{
    Q_OBJECT

private slots:
    void namesTheTransports()
    {
        QVERIFY(rc::isTransport(u"p2p"));
        QVERIFY(rc::isTransport(u"relay"));
        QVERIFY(!rc::isTransport(u""));
        QVERIFY(!rc::isTransport(u"udp"));
    }

    // 信令写着本机的 SDK 版本（给 0.1.1-beta 和以前的看）；读的时候不要它，transport、access 不认得的不是信令
    void modeRoundTrip()
    {
        const rc::ConnectionMode relay{rc::RelayTransport, rc::DeviceAccess};
        QJsonObject message;
        relay.write(message);
        QCOMPARE(message.value(QLatin1StringView("sdk")).toString(), rc::sdkVersion());
        QCOMPARE(rc::ConnectionMode::read(message), relay);
        message.remove(QStringLiteral("sdk"));
        QCOMPARE(rc::ConnectionMode::read(message), relay);
        message.remove(QStringLiteral("access"));
        QVERIFY(!rc::ConnectionMode::read(message));
    }

    void iceServersRoundTrip()
    {
        const rc::IceServers ice{{QStringLiteral("localhost:3478")},
                                 rc::TurnServer{QStringLiteral("localhost:3478"), true, QStringLiteral("1760000000:482915307"),
                                                QStringLiteral("secret")}};
        QCOMPARE(rc::IceServers::fromJson(ice.toJson()), ice);
        const rc::IceServers stunOnly{{QStringLiteral("a:3478"), QStringLiteral("b:3478")}, std::nullopt};
        QCOMPARE(rc::IceServers::fromJson(stunOnly.toJson()), stunOnly);
        QCOMPARE(stunOnly.stunServers(), (std::vector<std::string>{"a:3478", "b:3478"}));
    }

    // 服务器发来的样子（Server/src/protocol.ts 的 IceServers）
    void readsWhatTheServerSends()
    {
        const QJsonObject json{
            {QStringLiteral("stun"), QJsonArray{QStringLiteral("192.168.55.173:3478")}},
            {QStringLiteral("turn"), QJsonObject{{QStringLiteral("server"), QStringLiteral("192.168.55.173:3478")},
                                                 {QStringLiteral("transport"), QStringLiteral("udp")},
                                                 {QStringLiteral("username"), QStringLiteral("1760000000:482915307")},
                                                 {QStringLiteral("password"), QStringLiteral("pw")}}},
            {QStringLiteral("ttl"), 86400},
        };
        const rc::IceServers ice = rc::IceServers::fromJson(json);
        QCOMPARE(ice.stun, QStringList{QStringLiteral("192.168.55.173:3478")});
        QVERIFY(ice.turn);
        QCOMPARE(ice.turn->server, QStringLiteral("192.168.55.173:3478"));
        QVERIFY(!ice.turn->tcp);
        QCOMPARE(ice.turn->username, QStringLiteral("1760000000:482915307"));
        QCOMPARE(ice.turn->password, QStringLiteral("pw"));
        // 服务器没配中继
        QCOMPARE(rc::IceServers::fromJson({}), rc::IceServers{});
    }
};

QTEST_GUILESS_MAIN(TestTransport)
#include "tst_transport.moc"
