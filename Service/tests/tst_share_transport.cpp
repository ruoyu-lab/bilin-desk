#include "share_transport.h"

#include <QTest>

namespace {

using Verdict = ShareTransport::Verdict;

const QString Direct{rc::DirectTransport};
const QString Relay{rc::RelayTransport};
const QString Mac = QStringLiteral("111111111");
const QString Windows = QStringLiteral("222222222");

} // namespace

// 分享端换不换传输方式：check(来连的设备, 它要的方式, 有没有人在用, 有没有中继)
class TestShareTransport : public QObject
{
    Q_OBJECT

private slots:
    void startsDirect()
    {
        ShareTransport transport;
        QCOMPARE(transport.current(), Direct);
        // 一样的方式：有没有人在用都照常
        QVERIFY(transport.check(Mac, Direct, true, false) == Verdict::Accept);
    }

    void switchesWhenNobodyUsesIt()
    {
        ShareTransport transport;
        QVERIFY(transport.check(Mac, Relay, false, true) == Verdict::Switch);
        transport.switchTo(Relay);
        QCOMPARE(transport.current(), Relay);
        QVERIFY(transport.check(Mac, Relay, false, true) == Verdict::Accept);
        QVERIFY(transport.check(Windows, Direct, false, true) == Verdict::Switch);
    }

    void busyWhileSomeoneUsesIt()
    {
        ShareTransport transport;
        QVERIFY(transport.check(Mac, Relay, true, true) == Verdict::Busy);
        transport.switchTo(Relay);
        QVERIFY(transport.check(Windows, Direct, true, true) == Verdict::Busy);
    }

    void noRelayWithoutARelayServer()
    {
        ShareTransport transport;
        QVERIFY(transport.check(Mac, Relay, false, false) == Verdict::NoRelay);
        // 用着中继时凭据过期了：连着的照旧，新来要中继的不行
        transport.switchTo(Relay);
        QVERIFY(transport.check(Windows, Relay, true, false) == Verdict::NoRelay);
    }

    // 别的设备刚来连过、还没连通：换了会断掉它的连接
    void othersStillConnectingCountAsInUse()
    {
        ShareTransport transport(100);
        transport.attempting(Mac);
        QVERIFY(transport.check(Windows, Relay, false, true) == Verdict::Busy);
        // 它自己直连打不通、改走中继：可以换
        QVERIFY(transport.check(Mac, Relay, false, true) == Verdict::Switch);
        // 连通了：之后算在 inUse 里
        transport.arrived(Mac);
        QVERIFY(transport.check(Windows, Relay, false, true) == Verdict::Switch);
        // 一直没连通：过了时候就不算了
        transport.attempting(Mac);
        QTest::qWait(150);
        QVERIFY(transport.check(Windows, Relay, false, true) == Verdict::Switch);
    }

    void switchingDropsThoseStillConnecting()
    {
        ShareTransport transport;
        transport.attempting(Mac);
        transport.switchTo(Relay);
        QVERIFY(transport.check(Windows, Direct, false, true) == Verdict::Switch);
    }

    void switchesBackWhenTheRelayIsIdle()
    {
        ShareTransport transport(100);
        QVERIFY(!transport.shouldSwitchBack(false));
        transport.switchTo(Relay);
        transport.attempting(Mac);
        QVERIFY(!transport.shouldSwitchBack(false));
        transport.arrived(Mac);
        QVERIFY(!transport.shouldSwitchBack(true));
        QVERIFY(transport.shouldSwitchBack(false));
    }
};

QTEST_GUILESS_MAIN(TestShareTransport)
#include "tst_share_transport.moc"
