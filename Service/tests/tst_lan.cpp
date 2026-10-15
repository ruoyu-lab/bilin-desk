#include "device_store.h"
#include "test_keys.h"
#include "lan_discovery.h"
#include "lan_signaling.h"

#include <rc/lan_protocol.h>
#include <rc/transport.h>

#include <QJsonDocument>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

namespace {

// 测试用的端口：先占一个空闲的再放掉
quint16 freeTcpPort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost);
    return probe.serverPort();
}

quint16 freeUdpPort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress::LocalHost, 0);
    return probe.localPort();
}

} // namespace

// 局域网：附近的设备（发现）、不经服务器的信令
class TestLan : public QObject
{
    Q_OBJECT

private slots:
    // 测试直接往它的端口发 announce / leave（真的广播在测试机上不一定出得去）
    void discoveryListsWhatItHears()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const quint16 port = freeUdpPort();
        LanDiscovery discovery(store, {port, 60000, 300});
        QSignalSpy changed(&discovery, &LanDiscovery::changed);
        QUdpSocket peer;

        const rc::lan::Presence living{QStringLiteral("482915307"), QStringLiteral("客厅电脑"), QStringLiteral("Windows")};
        peer.writeDatagram(rc::lan::announce(living), QHostAddress::LocalHost, port);
        QTRY_COMPARE(discovery.state().devices.size(), 1);
        const auto device = discovery.state().devices.first();
        QCOMPARE(device.name, QStringLiteral("客厅电脑"));
        QCOMPARE(device.platform, QStringLiteral("Windows"));
        QCOMPARE(device.address, QStringLiteral("127.0.0.1"));
        QCOMPARE(discovery.addressOf(QStringLiteral("482915307")), QStringLiteral("127.0.0.1"));

        // 自己的、别的程序的不算
        peer.writeDatagram(rc::lan::announce({store.state().deviceId, QStringLiteral("本机"), {}}), QHostAddress::LocalHost, port);
        peer.writeDatagram(QByteArray("hello"), QHostAddress::LocalHost, port);
        QTest::qWait(100);
        QCOMPARE(discovery.state().devices.size(), 1);

        peer.writeDatagram(rc::lan::leave(living.deviceId), QHostAddress::LocalHost, port);
        QTRY_COMPARE(discovery.state().devices.size(), 0);
        QVERIFY(changed.count() >= 2);
    }

    void discoveryForgetsTheSilent()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const quint16 port = freeUdpPort();
        // 每 100 毫秒看一次，300 毫秒没听到就忘掉
        LanDiscovery discovery(store, {port, 100, 300});
        QUdpSocket peer;
        peer.writeDatagram(rc::lan::announce({QStringLiteral("482915307"), QStringLiteral("客厅电脑"), {}}),
                           QHostAddress::LocalHost, port);
        QTRY_COMPARE(discovery.state().devices.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(discovery.state().devices.size(), 0, 3000);
    }

    // 它说了默认走的那块网卡的地址：听得到从那里发来的就连它，听不到（别的网段）就还是连发来的地址
    void prefersTheAddressItNames()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const quint16 port = freeUdpPort();
        LanDiscovery discovery(store, {port, 60000, 3000});
        QUdpSocket peer;
        rc::lan::Presence living{QStringLiteral("482915307"), QStringLiteral("客厅电脑"), QStringLiteral("Windows"),
                                 QStringLiteral("10.9.9.9")};
        peer.writeDatagram(rc::lan::announce(living), QHostAddress::LocalHost, port);
        QTRY_COMPARE(discovery.addressOf(living.deviceId), QStringLiteral("127.0.0.1"));
        living.address = QStringLiteral("127.0.0.1");
        peer.writeDatagram(rc::lan::announce(living), QHostAddress::LocalHost, port);
        QTest::qWait(100);
        QCOMPARE(discovery.addressOf(living.deviceId), QStringLiteral("127.0.0.1"));
    }

    // 手机：只听不说，附近的电脑照样听得到
    void listeningOnlyStillHears()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const quint16 port = freeUdpPort();
        LanDiscovery discovery(store, {.port = port, .announceMs = 60000, .forgetMs = 300, .announce = false});
        QUdpSocket peer;
        peer.writeDatagram(rc::lan::announce({QStringLiteral("482915307"), QStringLiteral("客厅电脑"), {}}),
                           QHostAddress::LocalHost, port);
        QTRY_COMPARE(discovery.state().devices.size(), 1);
    }

    void discoveryOffMeansNothing()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        store.setLanDiscovery(false);
        const quint16 port = freeUdpPort();
        LanDiscovery discovery(store, {port, 60000, 300});
        QUdpSocket peer;
        peer.writeDatagram(rc::lan::announce({QStringLiteral("482915307"), QStringLiteral("客厅电脑"), {}}),
                           QHostAddress::LocalHost, port);
        QTest::qWait(200);
        QCOMPARE(discovery.state().devices.size(), 0);
    }

    // 两台"电脑"：viewer 去连 sharer，sharer 在同一条连接上回答、回绝
    void signalingGoesBothWays()
    {
        QTemporaryDir viewerDirectory;
        QTemporaryDir sharerDirectory;
        DeviceStore viewerStore(viewerDirectory.path(), testDeviceKey);
        DeviceStore sharerStore(sharerDirectory.path(), testDeviceKey);
        const quint16 sharerPort = freeTcpPort();
        LanSignaling sharer(sharerStore, {sharerPort, 0});
        QVERIFY(sharer.listen());
        LanSignaling viewer(viewerStore, {freeTcpPort(), sharerPort});

        QSignalSpy offered(&sharer, &LanSignaling::received);
        const rc::ConnectionMode relay{rc::RelayTransport, rc::DeviceAccess};
        viewer.send(QStringLiteral("127.0.0.1"), QStringLiteral("s1"), QStringLiteral("offer"), relay);
        QTRY_COMPARE(offered.count(), 1);
        QCOMPARE(offered.at(0), (QVariantList{QStringLiteral("127.0.0.1"), viewerStore.state().deviceId,
                                              viewerStore.state().deviceName, QStringLiteral("s1"), QStringLiteral("offer"),
                                              QVariant::fromValue(relay)}));

        QSignalSpy answered(&viewer, &LanSignaling::received);
        sharer.answer(QStringLiteral("s1"), QStringLiteral("answer"), relay);
        QTRY_COMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).toString(), sharerStore.state().deviceId);
        QCOMPARE(answered.at(0).at(4).toString(), QStringLiteral("answer"));
        QCOMPARE(answered.at(0).at(5).value<rc::ConnectionMode>(), relay);
        // 回答从去连的那条连接回来，不算别人来连本机
        QCOMPARE(offered.count(), 1);

        QSignalSpy declined(&viewer, &LanSignaling::declined);
        viewer.send(QStringLiteral("127.0.0.1"), QStringLiteral("s2"), QStringLiteral("offer"), rc::ConnectionMode{});
        QTRY_COMPARE(offered.count(), 2);
        sharer.decline(QStringLiteral("s2"), QStringLiteral("off"));
        QTRY_COMPARE(declined.count(), 1);
        QCOMPARE(declined.at(0), (QVariantList{sharerStore.state().deviceId, QStringLiteral("s2"), QStringLiteral("off")}));
    }

    // 没说怎么连（传输方式、怎么证明自己）的信令：断开，不交给分享会话
    void refusesSignalsWithoutAMode_data()
    {
        QTest::addColumn<QString>("missing");
        QTest::newRow("transport") << QStringLiteral("transport");
        QTest::newRow("access") << QStringLiteral("access");
    }

    void refusesSignalsWithoutAMode()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const quint16 port = freeTcpPort();
        LanSignaling sharer(store, {port, 0});
        QVERIFY(sharer.listen());
        QSignalSpy offered(&sharer, &LanSignaling::received);

        QTcpSocket peer;
        peer.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(peer.waitForConnected());
        QJsonObject signal = rc::lan::signal(QStringLiteral("482915307"), QStringLiteral("x"), QStringLiteral("s1"),
                                             QStringLiteral("offer"), rc::ConnectionMode{});
        QFETCH(QString, missing);
        signal.remove(missing);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("怎么连")));
        peer.write(QJsonDocument(signal).toJson(QJsonDocument::Compact) + '\n');
        QTRY_COMPARE(peer.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(offered.count(), 0);
    }

    void unreachableWhenNobodyListens()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        LanSignaling viewer(store, {freeTcpPort(), freeTcpPort()});
        QSignalSpy unreachable(&viewer, &LanSignaling::unreachable);
        viewer.send(QStringLiteral("127.0.0.1"), QStringLiteral("s1"), QStringLiteral("offer"), rc::ConnectionMode{});
        QTRY_COMPARE(unreachable.count(), 1);
        QCOMPARE(unreachable.at(0).at(0).toString(), QStringLiteral("s1"));
    }
};

QTEST_GUILESS_MAIN(TestLan)
#include "tst_lan.moc"
