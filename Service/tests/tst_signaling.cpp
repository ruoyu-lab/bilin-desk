#include "signaling.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

namespace {

const QString Viewer = QStringLiteral("222222222");
const QString Nearby = QStringLiteral("333333333");
const QString NearbyAddress = QStringLiteral("192.168.1.20");
const rc::ConnectionMode Direct{rc::DirectTransport, rc::PasswordAccess};
// 我的设备经中继
const rc::ConnectionMode Relay{rc::RelayTransport, rc::DeviceAccess};

// 附近只有 Nearby 一台
QString nearbyAddress(const QString &deviceId)
{
    return deviceId == Nearby ? NearbyAddress : QString();
}

} // namespace

class TestSignaling : public QObject
{
    Q_OBJECT

private slots:
    void farDevicesGoThroughTheServer()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy server(&signaling, &Signaling::toServer);
        QSignalSpy lan(&signaling, &Signaling::toLan);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        signaling.fromViewer(Viewer, {}, QStringLiteral("s1"), QStringLiteral("offer"), Relay);
        QCOMPARE(server.count(), 1);
        QCOMPARE(server.at(0), (QVariantList{Viewer, QStringLiteral("s1"), QStringLiteral("offer"), QVariant::fromValue(Relay)}));
        QCOMPARE(lan.count(), 0);
        signaling.fromServer(Viewer, QStringLiteral("客厅电脑"), QStringLiteral("s1"), QStringLiteral("answer"), Relay);
        QCOMPARE(viewer.count(), 1);
        QCOMPARE(viewer.at(0).at(2).toString(), QStringLiteral("answer"));
    }

    void nearbyDevicesGoThroughTheLan()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy server(&signaling, &Signaling::toServer);
        QSignalSpy lan(&signaling, &Signaling::toLan);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        signaling.fromViewer(Nearby, {}, QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        QCOMPARE(server.count(), 0);
        QCOMPARE(lan.count(), 1);
        QCOMPARE(lan.at(0), (QVariantList{NearbyAddress, QStringLiteral("s1"), QStringLiteral("offer"), QVariant::fromValue(Direct)}));
        signaling.fromLan(NearbyAddress, Nearby, QStringLiteral("书房"), QStringLiteral("s1"), QStringLiteral("answer"), Direct);
        QCOMPARE(viewer.count(), 1);
    }

    // 按 IP 地址连：不知道对面是哪台设备，第一个回答的就是它；之后只认它
    void addressesGoThroughTheLan()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy lan(&signaling, &Signaling::toLan);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        signaling.fromViewer({}, QStringLiteral("10.0.0.8"), QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        QCOMPARE(lan.at(0).at(0).toString(), QStringLiteral("10.0.0.8"));
        signaling.fromLan(QStringLiteral("10.0.0.8"), QStringLiteral("444444444"), QStringLiteral("x"), QStringLiteral("s1"),
                          QStringLiteral("answer"), Direct);
        QCOMPARE(viewer.count(), 1);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("冒用")));
        signaling.fromLan(QStringLiteral("10.0.0.8"), QStringLiteral("555555555"), QStringLiteral("x"), QStringLiteral("s1"),
                          QStringLiteral("answer"), Direct);
        QCOMPARE(viewer.count(), 1);
    }

    // 来连本机的：从哪条路来，回答、回绝就从哪条路回去
    void sharerAnswersTheWayTheOfferCame()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy sharer(&signaling, &Signaling::toSharer);
        QSignalSpy server(&signaling, &Signaling::toServer);
        QSignalSpy lanAnswer(&signaling, &Signaling::answerToLan);
        QSignalSpy serverDecline(&signaling, &Signaling::declineToServer);
        QSignalSpy lanDecline(&signaling, &Signaling::declineToLan);

        signaling.fromServer(Viewer, QStringLiteral("小林的笔记本"), QStringLiteral("far"), QStringLiteral("offer"), Relay);
        signaling.fromLan(NearbyAddress, Nearby, QStringLiteral("书房"), QStringLiteral("near"), QStringLiteral("offer"), Direct);
        QCOMPARE(sharer.count(), 2);
        QCOMPARE(sharer.at(0), (QVariantList{Viewer, QStringLiteral("小林的笔记本"), QStringLiteral("far"),
                                             QStringLiteral("offer"), QVariant::fromValue(Relay)}));

        signaling.fromSharer(Viewer, QStringLiteral("far"), QStringLiteral("answer"));
        signaling.fromSharer(Nearby, QStringLiteral("near"), QStringLiteral("answer"));
        // 回答带着对方来时的那样（传输方式、怎么证明自己）
        QCOMPARE(server.count(), 1);
        QCOMPARE(server.at(0), (QVariantList{Viewer, QStringLiteral("far"), QStringLiteral("answer"), QVariant::fromValue(Relay)}));
        QCOMPARE(lanAnswer.count(), 1);
        QCOMPARE(lanAnswer.at(0), (QVariantList{QStringLiteral("near"), QStringLiteral("answer"), QVariant::fromValue(Direct)}));

        signaling.declineFromSharer(Viewer, QStringLiteral("far"), QStringLiteral("off"));
        signaling.declineFromSharer(Nearby, QStringLiteral("near"), QStringLiteral("off"));
        QCOMPARE(serverDecline.count(), 1);
        QCOMPARE(lanDecline.count(), 1);
    }

    // 连着的时候换了网络还会来信令：界面的连接到界面说结束才忘掉，之后同一个编号就当成别人来连本机
    void viewerSessionsLastUntilTheyEnd()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        QSignalSpy sharer(&signaling, &Signaling::toSharer);
        QSignalSpy forgotten(&signaling, &Signaling::forgotten);
        signaling.fromViewer(Viewer, {}, QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        signaling.fromServer(Viewer, QStringLiteral("x"), QStringLiteral("s1"), QStringLiteral("restart"), Direct);
        QCOMPARE(viewer.count(), 1);
        signaling.endViewer(QStringLiteral("s1"));
        QCOMPARE(forgotten.count(), 1);
        signaling.fromServer(Viewer, QStringLiteral("x"), QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        QCOMPARE(viewer.count(), 1);
        QCOMPARE(sharer.count(), 1);
    }

    // 按 IP 地址连的：之后的信令里设备 ID 还是空的，第一个回答认下的那台照旧认
    void addressSessionsKeepTheDeviceThatAnswered()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        signaling.fromViewer({}, QStringLiteral("10.0.0.8"), QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        signaling.fromLan(QStringLiteral("10.0.0.8"), QStringLiteral("444444444"), QStringLiteral("x"), QStringLiteral("s1"),
                          QStringLiteral("answer"), Direct);
        signaling.fromViewer({}, QStringLiteral("10.0.0.8"), QStringLiteral("s1"), QStringLiteral("restart"), Direct);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("冒用")));
        signaling.fromLan(QStringLiteral("10.0.0.8"), QStringLiteral("555555555"), QStringLiteral("x"), QStringLiteral("s1"),
                          QStringLiteral("answer"), Direct);
        QCOMPARE(viewer.count(), 1);
    }

    // 来连本机、连上了的：本机的回答一直从它来的那条路回去，直到那个人走了
    void sharerSessionsLastWhileConnected()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy lanAnswer(&signaling, &Signaling::answerToLan);
        QSignalSpy forgotten(&signaling, &Signaling::forgotten);
        signaling.fromLan(NearbyAddress, Nearby, QStringLiteral("书房"), QStringLiteral("near"), QStringLiteral("offer"), Relay);
        signaling.keepSharer(QStringLiteral("near"));
        signaling.fromSharer(Nearby, QStringLiteral("near"), QStringLiteral("restart"));
        QCOMPARE(lanAnswer.count(), 1);
        QCOMPARE(lanAnswer.at(0).at(2).value<rc::ConnectionMode>(), Relay);
        signaling.releaseSharer(QStringLiteral("near"));
        QCOMPARE(forgotten.count(), 1);
        QCOMPARE(forgotten.at(0).at(0).toString(), QStringLiteral("near"));
    }

    void answersFromAnotherDeviceAreIgnored()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy viewer(&signaling, &Signaling::toViewer);
        QSignalSpy sharer(&signaling, &Signaling::toSharer);
        signaling.fromViewer(Viewer, {}, QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("冒用")));
        signaling.fromServer(QStringLiteral("444444444"), QStringLiteral("别人"), QStringLiteral("s1"), QStringLiteral("fake"), Direct);
        QCOMPARE(viewer.count(), 0);
        QCOMPARE(sharer.count(), 0);
    }

    void undeliverableReachesOnlyThatViewer()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy undeliverable(&signaling, &Signaling::viewerUndeliverable);
        signaling.fromViewer(Viewer, {}, QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        signaling.undeliverable(QStringLiteral("555555555"), QStringLiteral("s1"), QStringLiteral("offline"));
        signaling.undeliverable(Viewer, QStringLiteral("other"), QStringLiteral("offline"));
        QCOMPARE(undeliverable.count(), 0);
        signaling.undeliverable(Viewer, QStringLiteral("s1"), QStringLiteral("noServer"));
        QCOMPARE(undeliverable.count(), 1);
        QCOMPARE(undeliverable.at(0).at(2).toString(), QStringLiteral("noServer"));
    }

    void unreachableOnTheLan()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy undeliverable(&signaling, &Signaling::viewerUndeliverable);
        signaling.fromViewer({}, QStringLiteral("10.0.0.8"), QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        signaling.lanUnreachable(QStringLiteral("s1"));
        QCOMPARE(undeliverable.count(), 1);
        QCOMPARE(undeliverable.at(0).at(2).toString(), QStringLiteral("unreachable"));
    }

    void declinesReachOnlyThatViewer()
    {
        Signaling signaling(nearbyAddress);
        QSignalSpy declined(&signaling, &Signaling::viewerDeclined);
        signaling.fromViewer(Viewer, {}, QStringLiteral("s1"), QStringLiteral("offer"), Direct);
        // 别的设备、别的连接回绝的不算
        signaling.declinedByPeer(QStringLiteral("555555555"), QStringLiteral("s1"), QStringLiteral("off"));
        signaling.declinedByPeer(Viewer, QStringLiteral("other"), QStringLiteral("off"));
        QCOMPARE(declined.count(), 0);
        signaling.declinedByPeer(Viewer, QStringLiteral("s1"), QStringLiteral("permission"));
        QCOMPARE(declined.count(), 1);
        QCOMPARE(declined.at(0), (QVariantList{Viewer, QStringLiteral("s1"), QStringLiteral("permission")}));
    }
};

QTEST_GUILESS_MAIN(TestSignaling)
#include "tst_signaling.moc"
