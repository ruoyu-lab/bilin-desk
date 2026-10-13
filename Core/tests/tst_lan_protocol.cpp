#include <rc/lan_protocol.h>

#include <QJsonDocument>
#include <QTest>

class TestLanProtocol : public QObject
{
    Q_OBJECT

private slots:
    void announceRoundTrips()
    {
        const rc::lan::Presence presence{QStringLiteral("482915307"), QStringLiteral("客厅电脑"), QStringLiteral("Windows")};
        QCOMPARE(rc::lan::parsePresence(rc::lan::announce(presence)), presence);
    }

    void leaveHasNoName()
    {
        const auto presence = rc::lan::parsePresence(rc::lan::leave(QStringLiteral("482915307")));
        QVERIFY(presence);
        QCOMPARE(presence->deviceId, QStringLiteral("482915307"));
        QVERIFY(presence->name.isEmpty());
    }

    void rejectsOthers_data()
    {
        QTest::addColumn<QByteArray>("datagram");
        QTest::newRow("not json") << QByteArray("hello");
        QTest::newRow("another app") << QByteArray(R"({"app":"other","type":"announce","deviceId":"482915307","name":"x"})");
        QTest::newRow("bad id") << QByteArray(R"({"app":"bilin","type":"announce","deviceId":"12","name":"x"})");
        QTest::newRow("no name") << QByteArray(R"({"app":"bilin","type":"announce","deviceId":"482915307"})");
        QTest::newRow("unknown type") << QByteArray(R"({"app":"bilin","type":"ping","deviceId":"482915307"})");
    }

    void rejectsOthers()
    {
        QFETCH(QByteArray, datagram);
        QVERIFY(!rc::lan::parsePresence(datagram));
    }
};

QTEST_GUILESS_MAIN(TestLanProtocol)
#include "tst_lan_protocol.moc"
