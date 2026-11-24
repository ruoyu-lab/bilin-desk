#include <rc/connection_report.h>
#include <rc/transport.h>

#include <QJsonArray>
#include <QTest>

using Route = rc::ConnectionReport::Route;

class TestConnectionReport : public QObject
{
    Q_OBJECT

private slots:
    // 直连不通走中继，UDP 不通走 TLS，连上、重连过一次，用了 10 分钟后自己断开
    void connectedThenClosed()
    {
        rc::ConnectionReport report;
        QVERIFY(!report.active());
        report.begin(QStringLiteral("731204586"), 1000);
        QVERIFY(report.active());
        report.attempt(Route::Direct, rc::DeviceAccess);
        report.attempt(Route::Relay, rc::DeviceAccess);
        report.attempt(Route::Relay, rc::DeviceAccess);
        report.attempt(Route::RelayTls, rc::DeviceAccess);
        report.connected(rc::NetworkPath::Relay, 7200);
        report.controlled();
        report.sample(60, 8, 40, QStringLiteral("HEVC"), QStringLiteral("1920 × 1080"));
        report.sample(50, 4, 60, QStringLiteral("HEVC"), QStringLiteral("2560 × 1440"));
        report.reconnecting();
        report.connected(rc::NetworkPath::Unknown, 9000);
        const QJsonObject json = report.finish({}, 7200 + 600 * 1000);
        QVERIFY(!report.active());
        QCOMPARE(json.value(u"target").toString(), QStringLiteral("731204586"));
        QCOMPARE(json.value(u"access").toString(), QString(rc::DeviceAccess));
        QCOMPARE(json.value(u"result").toString(), QStringLiteral("connected"));
        QCOMPARE(json.value(u"reason").toString(), QString());
        QCOMPARE(json.value(u"routes").toArray(),
                 (QJsonArray{QStringLiteral("direct"), QStringLiteral("relay"), QStringLiteral("relayTls")}));
        QCOMPARE(json.value(u"path").toString(), QStringLiteral("relay"));
        QCOMPARE(json.value(u"connectMs").toInteger(), 6200);
        QCOMPARE(json.value(u"durationS").toInteger(), 600);
        QCOMPARE(json.value(u"reconnects").toInt(), 1);
        QCOMPARE(json.value(u"control").toBool(), true);
        QCOMPARE(json.value(u"approval").toBool(), false);
        QCOMPARE(json.value(u"codec").toString(), QStringLiteral("HEVC"));
        QCOMPARE(json.value(u"resolution").toString(), QStringLiteral("2560 × 1440"));
        QCOMPARE(json.value(u"fps").toDouble(), 55.0);
        QCOMPARE(json.value(u"mbps").toDouble(), 6.0);
        QCOMPARE(json.value(u"latencyMs").toDouble(), 50.0);
    }

    void failedOrCancelled()
    {
        rc::ConnectionReport report;
        report.begin({}, 0);
        report.attempt(Route::Direct, rc::PasswordAccess);
        report.waiting();
        const QJsonObject failed = report.finish(QStringLiteral("对方拒绝了连接"), 5000);
        QCOMPARE(failed.value(u"target").toString(), QString());
        QCOMPARE(failed.value(u"result").toString(), QStringLiteral("failed"));
        QCOMPARE(failed.value(u"reason").toString(), QStringLiteral("对方拒绝了连接"));
        QCOMPARE(failed.value(u"approval").toBool(), true);
        QCOMPARE(failed.value(u"path").toString(), QString());
        QCOMPARE(failed.value(u"connectMs").toInteger(), -1);
        QCOMPARE(failed.value(u"durationS").toInteger(), 0);
        QCOMPARE(failed.value(u"fps").toDouble(), 0.0);

        // 新的一次从头记
        report.begin(QStringLiteral("731204586"), 10000);
        report.attempt(Route::Direct, rc::DeviceAccess);
        const QJsonObject cancelled = report.finish({}, 11000);
        QCOMPARE(cancelled.value(u"result").toString(), QStringLiteral("cancelled"));
        QCOMPARE(cancelled.value(u"approval").toBool(), false);
        QCOMPARE(cancelled.value(u"routes").toArray(), QJsonArray{QStringLiteral("direct")});
    }

    // 服务器收的上限：原因 200 个字、路 8 段
    void staysWithinTheServersLimits()
    {
        rc::ConnectionReport report;
        report.begin({}, 0);
        for (int i = 0; i < 10; ++i) {
            report.attempt(Route::Direct, rc::PasswordAccess);
            report.attempt(Route::Relay, rc::PasswordAccess);
        }
        const QJsonObject json = report.finish(QString(300, u'x'), 1);
        QCOMPARE(json.value(u"routes").toArray().size(), 8);
        QCOMPARE(json.value(u"reason").toString().size(), 200);
    }
};

QTEST_GUILESS_MAIN(TestConnectionReport)
#include "tst_connection_report.moc"
