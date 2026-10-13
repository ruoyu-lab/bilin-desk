#include <rc/network_path.h>

#include <QTest>

Q_DECLARE_METATYPE(rc::NetworkPath)

class TestNetworkPath : public QObject
{
    Q_OBJECT

private slots:
    void classifies_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<rc::NetworkPath>("kind");
        QTest::newRow("lan") << QStringLiteral("P2P 0.0.0.0:52011 <-> 192.168.55.143:61822 (host)")
                             << rc::NetworkPath::Lan;
        QTest::newRow("lan ipv6") << QStringLiteral("P2P [::]:52011 <-> [fe80::1c2b:3aff:fe11:2233]:61822 (host)")
                                  << rc::NetworkPath::Lan;
        QTest::newRow("hole punched") << QStringLiteral("P2P 0.0.0.0:52011 <-> 203.0.113.7:6000 (srflx)")
                                      << rc::NetworkPath::Direct;
        QTest::newRow("peer reflexive") << QStringLiteral("P2P 0.0.0.0:52011 <-> 198.51.100.20:6000 (prflx)")
                                        << rc::NetworkPath::Direct;
        // 对方有公网地址
        QTest::newRow("public host") << QStringLiteral("P2P [::]:52011 <-> [2001:db8::5]:6000 (host)")
                                     << rc::NetworkPath::Direct;
        QTest::newRow("peer's relay") << QStringLiteral("P2P 0.0.0.0:52011 <-> 198.51.100.9:49152 (relay)")
                                      << rc::NetworkPath::Relay;
        QTest::newRow("own relay")
            << QStringLiteral("relay 198.51.100.9:49152 on TURN 198.51.100.9:3478 (over TLS) <-> 203.0.113.7:6000 (srflx)")
            << rc::NetworkPath::Relay;
        QTest::newRow("not connected") << QString() << rc::NetworkPath::Unknown;
        QTest::newRow("other transport") << QStringLiteral("host datagram channel") << rc::NetworkPath::Unknown;
    }

    void classifies()
    {
        QFETCH(QString, path);
        QFETCH(rc::NetworkPath, kind);
        QCOMPARE(rc::networkPath(path), kind);
    }
};

QTEST_GUILESS_MAIN(TestNetworkPath)
#include "tst_network_path.moc"
