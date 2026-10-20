#include "device_store.h"
#include "test_keys.h"
#include "server_link.h"

#include <rc/json_file.h>
#include <rc/machine.h>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSslConfiguration>
#include <QSslKey>
#include <QTemporaryDir>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

// 测试里的"服务器"：记下收到的 hello，按测试的意思回答
class FakeServer : public QObject
{
    Q_OBJECT

public:
    QWebSocketServer server;
    QList<QJsonObject> hellos;
    QWebSocket *current = nullptr;
    // 收到 hello 以后怎么办；默认欢迎
    std::function<void(QWebSocket *, const QJsonObject &)> answer = [](QWebSocket *socket, const QJsonObject &) {
        send(socket, {{QStringLiteral("type"), QStringLiteral("welcome")}});
    };

    // certificate：wss://，用 tls/ 里的这一张证书（测试用的，只给 127.0.0.1）；空的是 ws://
    explicit FakeServer(const QString &certificate = {})
        : server(QStringLiteral("test"), certificate.isEmpty() ? QWebSocketServer::NonSecureMode : QWebSocketServer::SecureMode)
    {
        if (!certificate.isEmpty()) {
            QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
            tls.setLocalCertificate(QSslCertificate(readTlsFile(certificate + QStringLiteral(".crt"))));
            tls.setPrivateKey(QSslKey(readTlsFile(certificate + QStringLiteral(".key")), QSsl::Rsa));
            tls.setPeerVerifyMode(QSslSocket::VerifyNone);
            server.setSslConfiguration(tls);
        }
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QWebSocketServer::newConnection, this, [this] {
            QWebSocket *socket = server.nextPendingConnection();
            socket->setParent(this);
            current = socket;
            connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString &text) {
                const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
                hellos.append(message);
                answer(socket, message);
            });
        });
    }

    QUrl url() const { return server.serverUrl(); }

    static QByteArray readTlsFile(const QString &name)
    {
        QFile file(QStringLiteral(TEST_TLS_DIR "/") + name);
        if (!file.open(QIODevice::ReadOnly))
            qFatal("没有测试用的证书文件 %s", qUtf8Printable(name));
        return file.readAll();
    }

    static void send(QWebSocket *socket, const QJsonObject &message)
    {
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }
};

class TestServerLink : public QObject
{
    Q_OBJECT

private:
    static constexpr ServerLink::Options Fast{20, 100, 30000, 75000};

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
    }

    void logsInWithIdAndToken()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        QCOMPARE(link.state(), ServerLink::State::Connecting);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        QCOMPARE(fake.hellos.size(), 1);
        const QJsonObject hello = fake.hellos.first();
        QCOMPARE(hello.value(u"type").toString(), QStringLiteral("hello"));
        QCOMPARE(hello.value(u"deviceId").toString(), store.state().deviceId);
        QCOMPARE(hello.value(u"token").toString(), store.deviceToken());
        QCOMPARE(QByteArray::fromBase64(hello.value(u"publicKey").toString().toLatin1()), store.publicKey());
        QCOMPARE(hello.value(u"version").toString(), QStringLiteral("0.1"));
        // 服务器没配中继
        QCOMPARE(link.ice(), rc::IceServers{});
    }

    void followsTheIceServers()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        const auto ice = [](const QString &password) {
            return QJsonObject{
                {QStringLiteral("stun"), QJsonArray{QStringLiteral("localhost:3478")}},
                {QStringLiteral("turn"), QJsonObject{{QStringLiteral("server"), QStringLiteral("localhost:3478")},
                                                     {QStringLiteral("transport"), QStringLiteral("tcp")},
                                                     {QStringLiteral("username"), QStringLiteral("1760000000:482915307")},
                                                     {QStringLiteral("password"), password}}},
                {QStringLiteral("ttl"), 1},
            };
        };
        fake.answer = [&ice](QWebSocket *socket, const QJsonObject &) {
            FakeServer::send(socket, {{QStringLiteral("type"), QStringLiteral("welcome")}, {QStringLiteral("ice"), ice(QStringLiteral("one"))}});
        };
        ServerLink link(store, fake.url(), Fast);
        QSignalSpy changed(&link, &ServerLink::iceChanged);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        // 登录上的时候已经有了
        QCOMPARE(changed.count(), 1);
        const rc::TurnServer turn{QStringLiteral("localhost:3478"), true, QStringLiteral("1760000000:482915307"),
                                  QStringLiteral("one")};
        QCOMPARE(link.ice(), (rc::IceServers{{QStringLiteral("localhost:3478")}, turn}));

        // 快过期时服务器发来新的凭据
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("ice")}, {QStringLiteral("ice"), ice(QStringLiteral("two"))}});
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(link.ice().turn.value_or(rc::TurnServer{}).password, QStringLiteral("two"));

        // 到期了还没有新的：中继不能用了，STUN 照旧
        QTRY_COMPARE(changed.count(), 3);
        QVERIFY(!link.ice().turn);
        QCOMPARE(link.ice().stun, QStringList{QStringLiteral("localhost:3478")});
    }

    void takenIdIsReplacedAndRegisteredAgain()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const QString first = store.state().deviceId;
        FakeServer fake;
        fake.answer = [&fake](QWebSocket *socket, const QJsonObject &) {
            if (fake.hellos.size() == 1) {
                FakeServer::send(socket, {{QStringLiteral("type"), QStringLiteral("rejected")},
                                          {QStringLiteral("reason"), QStringLiteral("idTaken")}});
                socket->close(QWebSocketProtocol::CloseCode(4001));
                return;
            }
            FakeServer::send(socket, {{QStringLiteral("type"), QStringLiteral("welcome")}});
        };
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        QCOMPARE(fake.hellos.size(), 2);
        const QString second = fake.hellos.at(1).value(u"deviceId").toString();
        QVERIFY(second != first);
        QCOMPARE(store.state().deviceId, second);
        QCOMPARE(fake.hellos.at(1).value(u"token").toString(), fake.hellos.at(0).value(u"token").toString());
        // 新的 ID 存下来了
        QCOMPARE(rc::readJsonFile(directory.filePath(QStringLiteral("identity.json"))).value(u"deviceId").toString(), second);
    }

    // 登录账号时服务器认出本机是原来的那台电脑：换回原来的 ID、同一个令牌再登录，存下来
    void takesTheOldIdBack()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        fake.answer = [&fake](QWebSocket *socket, const QJsonObject &) {
            FakeServer::send(socket, {{QStringLiteral("type"), QStringLiteral("welcome")}});
            if (fake.hellos.size() == 1) {
                FakeServer::send(socket, {{QStringLiteral("type"), QStringLiteral("renumbered")},
                                          {QStringLiteral("deviceId"), QStringLiteral("731204586")}});
                socket->close(QWebSocketProtocol::CloseCode(4005));
            }
        };
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(fake.hellos.size(), 2);
        QCOMPARE(fake.hellos.at(1).value(u"deviceId").toString(), QStringLiteral("731204586"));
        QCOMPARE(fake.hellos.at(1).value(u"token").toString(), fake.hellos.at(0).value(u"token").toString());
        QCOMPARE(fake.hellos.at(0).value(u"machine").toString(), rc::machineHash());
        QCOMPARE(store.state().deviceId, QStringLiteral("731204586"));
        QCOMPARE(rc::readJsonFile(directory.filePath(QStringLiteral("identity.json"))).value(u"deviceId").toString(),
                 QStringLiteral("731204586"));
    }

    void reconnectsWhenTheServerGoesAway()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        // 离线只有重试前的一小会儿（20 毫秒）：记下经过的每个状态，不去碰巧看到它
        QList<ServerLink::State> states;
        connect(&link, &ServerLink::stateChanged, this, [&] { states.append(link.state()); });
        fake.current->close();
        QTRY_COMPARE(fake.hellos.size(), 2);
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        QVERIFY(states.contains(ServerLink::State::Offline));
    }

    void staysOfflineAfterBeingReplaced()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("别处登录")));
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("replaced")}});
        fake.current->close(QWebSocketProtocol::CloseCode(4003));
        QTRY_COMPARE(link.state(), ServerLink::State::Offline);
        QTest::qWait(300);
        QCOMPARE(fake.hellos.size(), 1);
    }

    void sendsAndReceivesSignals()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        const rc::ConnectionMode relay{rc::RelayTransport, rc::DeviceAccess};
        // 没登录时发不出去
        QVERIFY(!link.sendSignal(QStringLiteral("222222222"), QStringLiteral("s1"), QStringLiteral("offer"), relay));
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        QVERIFY(link.sendSignal(QStringLiteral("222222222"), QStringLiteral("s1"), QStringLiteral("offer"), relay));
        QTRY_COMPARE(fake.hellos.size(), 2);
        QCOMPARE(fake.hellos.at(1), (QJsonObject{{QStringLiteral("type"), QStringLiteral("signal")},
                                                 {QStringLiteral("to"), QStringLiteral("222222222")},
                                                 {QStringLiteral("session"), QStringLiteral("s1")},
                                                 {QStringLiteral("text"), QStringLiteral("offer")},
                                                 {QStringLiteral("transport"), relay.transport},
                                                 {QStringLiteral("access"), relay.access},
                                                 {QStringLiteral("sdk"), rc::sdkVersion()}}));

        QSignalSpy received(&link, &ServerLink::signalReceived);
        QSignalSpy undeliverable(&link, &ServerLink::undeliverable);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("signal")},
                                        {QStringLiteral("from"), QStringLiteral("222222222")},
                                        {QStringLiteral("name"), QStringLiteral("客厅电脑")},
                                        {QStringLiteral("session"), QStringLiteral("s1")},
                                        {QStringLiteral("text"), QStringLiteral("answer")},
                                        {QStringLiteral("transport"), relay.transport},
                                        {QStringLiteral("access"), relay.access},
                                        {QStringLiteral("sdk"), rc::sdkVersion()}});
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("undeliverable")},
                                        {QStringLiteral("to"), QStringLiteral("333333333")},
                                        {QStringLiteral("session"), QStringLiteral("s2")},
                                        {QStringLiteral("reason"), QStringLiteral("offline")}});
        QTRY_COMPARE(undeliverable.count(), 1);
        QCOMPARE(received.count(), 1);
        QCOMPARE(received.at(0).at(4).value<rc::ConnectionMode>(), relay);

        // 回绝：发出去的、收到的
        link.sendDecline(QStringLiteral("444444444"), QStringLiteral("s3"), QStringLiteral("off"));
        QTRY_COMPARE(fake.hellos.size(), 3);
        QCOMPARE(fake.hellos.at(2), (QJsonObject{{QStringLiteral("type"), QStringLiteral("decline")},
                                                 {QStringLiteral("to"), QStringLiteral("444444444")},
                                                 {QStringLiteral("session"), QStringLiteral("s3")},
                                                 {QStringLiteral("reason"), QStringLiteral("off")}}));
        QSignalSpy declined(&link, &ServerLink::declined);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("decline")},
                                        {QStringLiteral("from"), QStringLiteral("222222222")},
                                        {QStringLiteral("session"), QStringLiteral("s1")},
                                        {QStringLiteral("reason"), QStringLiteral("permission")}});
        QTRY_COMPARE(declined.count(), 1);
        QCOMPARE(declined.at(0), (QVariantList{QStringLiteral("222222222"), QStringLiteral("s1"), QStringLiteral("permission")}));
        QCOMPARE(received.at(0), (QVariantList{QStringLiteral("222222222"), QStringLiteral("客厅电脑"),
                                               QStringLiteral("s1"), QStringLiteral("answer"), QVariant::fromValue(relay)}));
        QCOMPARE(undeliverable.at(0),
                 (QVariantList{QStringLiteral("333333333"), QStringLiteral("s2"), QStringLiteral("offline")}));
    }

    void tellsTheServerANewName()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        store.setDeviceName(QStringLiteral("书房的电脑"));
        QTRY_COMPARE(fake.hellos.size(), 2);
        QCOMPARE(fake.hellos.at(1), (QJsonObject{{QStringLiteral("type"), QStringLiteral("rename")},
                                                 {QStringLiteral("name"), QStringLiteral("书房的电脑")}}));
        // 别的设置变了不用告诉服务器
        store.setLanDiscovery(false);
        QTest::qWait(100);
        QCOMPARE(fake.hellos.size(), 2);
    }

    void followsTheAccount()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        // 登录上了，服务器还没说账号：不知道信任谁
        QVERIFY(!link.trustedKeys());

        QVERIFY(link.signIn(QStringLiteral("li@example.com"), QStringLiteral("password1")));
        QTRY_COMPARE(fake.hellos.size(), 2);
        QCOMPARE(fake.hellos.at(1), (QJsonObject{{QStringLiteral("type"), QStringLiteral("login")},
                                                 {QStringLiteral("email"), QStringLiteral("li@example.com")},
                                                 {QStringLiteral("password"), QStringLiteral("password1")}}));
        // 改昵称、改密码、注销照原样发出去
        QVERIFY(link.setNickname(QStringLiteral("新昵称")));
        QVERIFY(link.changePassword(QStringLiteral("password1"), QStringLiteral("password2")));
        QVERIFY(link.deleteAccount(QStringLiteral("password2")));
        QTRY_COMPARE(fake.hellos.size(), 5);
        QCOMPARE(fake.hellos.at(2).value(u"type").toString(), QStringLiteral("setNickname"));
        QCOMPARE(fake.hellos.at(3).value(u"type").toString(), QStringLiteral("changePassword"));
        QCOMPARE(fake.hellos.at(4).value(u"type").toString(), QStringLiteral("deleteAccount"));
        QSignalSpy passwordChanged(&link, &ServerLink::passwordChanged);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("passwordChanged")}});
        QTRY_COMPARE(passwordChanged.count(), 1);

        QSignalSpy changed(&link, &ServerLink::accountChanged);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("account")},
                                        {QStringLiteral("account"), QJsonObject{{QStringLiteral("email"), QStringLiteral("li@example.com")},
                                                                                {QStringLiteral("nickname"), QStringLiteral("梨茶")}}}});
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("devices")},
                                        {QStringLiteral("devices"), QJsonArray{QJsonObject{
                                                                        {QStringLiteral("deviceId"), QStringLiteral("222222222")},
                                                                        {QStringLiteral("name"), QStringLiteral("客厅电脑")},
                                                                        {QStringLiteral("platform"), QStringLiteral("Windows")},
                                                                        {QStringLiteral("publicKey"), QStringLiteral("key")},
                                                                        {QStringLiteral("online"), true}}}}});
        QTRY_COMPARE(changed.count(), 2);
        QVERIFY(link.account().signedIn);
        QCOMPARE(link.account().nickname, QStringLiteral("梨茶"));
        QCOMPARE(link.account().devices.size(), 1);
        QVERIFY(link.account().devices.first().online);
        // 公钥不对（"key" 不是 65 字节）的不信任
        QVERIFY(link.trustedKeys() == QList<QByteArray>{});

        QSignalSpy failed(&link, &ServerLink::accountFailed);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("accountFailed")},
                                        {QStringLiteral("reason"), QStringLiteral("wrongPassword")}});
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("wrongPassword"));

        // 和服务器断了：账号还在，别的设备在不在线不知道了，都算不在线
        fake.current->close();
        QTRY_VERIFY(!link.account().devices.first().online);
        QVERIFY(link.account().signedIn);
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        // 信任的是账号下别的设备的公钥，不算本机
        const QByteArray other(DeviceStore::PublicKeySize, '\x04');
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("devices")},
                                        {QStringLiteral("devices"), QJsonArray{
                                            QJsonObject{{QStringLiteral("deviceId"), QStringLiteral("222222222")},
                                                        {QStringLiteral("name"), QStringLiteral("客厅电脑")},
                                                        {QStringLiteral("platform"), QStringLiteral("Windows")},
                                                        {QStringLiteral("publicKey"), QString::fromLatin1(other.toBase64())},
                                                        {QStringLiteral("online"), true}},
                                            QJsonObject{{QStringLiteral("deviceId"), store.state().deviceId},
                                                        {QStringLiteral("name"), QStringLiteral("本机")},
                                                        {QStringLiteral("platform"), QStringLiteral("macOS")},
                                                        {QStringLiteral("publicKey"), QString::fromLatin1(store.publicKey().toBase64())},
                                                        {QStringLiteral("online"), true}}}}});
        QTRY_VERIFY(link.trustedKeys() == QList<QByteArray>{other});

        // 退出了：账号、设备都不要了
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("account")}, {QStringLiteral("account"), QJsonValue()}});
        QTRY_VERIFY(!link.account().signedIn);
        QVERIFY(link.account().devices.isEmpty());
        QVERIFY(link.trustedKeys() == QList<QByteArray>{});
    }

    // 远程唤醒：登录着账号时把本机能唤醒它的网卡告诉服务器，每条连接、每个账号说一次，变了再说
    void tellsItsNetworkCardsWhileSignedIn()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        const auto told = [&fake] {
            QList<QJsonArray> interfaces;
            for (const QJsonObject &message : std::as_const(fake.hellos)) {
                if (message.value(u"type").toString() == u"wakeInfo")
                    interfaces.append(message.value(u"interfaces").toArray());
            }
            return interfaces;
        };
        const QJsonObject signedIn{{QStringLiteral("type"), QStringLiteral("account")},
                                   {QStringLiteral("account"), QJsonObject{{QStringLiteral("email"), QStringLiteral("li@example.com")},
                                                                           {QStringLiteral("nickname"), QStringLiteral("梨茶")}}}};
        rc::server::WakeInterface wired;
        wired.mac = QByteArray::fromHex("aabbccddee01");
        wired.address = QStringLiteral("192.168.1.20");
        wired.prefix = 24;
        wired.wake = rc::server::WakeInterface::Setting::Enabled;

        // 没登录账号：不说
        link.setWakeInterfaces({wired});
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("account")}, {QStringLiteral("account"), QJsonValue()}});
        QTRY_VERIFY(link.account().known);
        QVERIFY(told().isEmpty());
        // 登录了：说一次；账号下的设备有变化（又来一次 account）不再说
        FakeServer::send(fake.current, signedIn);
        QTRY_COMPARE(told().size(), 1);
        QCOMPARE(told().first(), (QJsonArray{QJsonObject{{QStringLiteral("mac"), QStringLiteral("aa:bb:cc:dd:ee:01")},
                                                         {QStringLiteral("address"), QStringLiteral("192.168.1.20")},
                                                         {QStringLiteral("prefix"), 24},
                                                         {QStringLiteral("link"), QStringLiteral("ethernet")},
                                                         {QStringLiteral("wake"), QStringLiteral("enabled")}}}));
        FakeServer::send(fake.current, signedIn);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("passwordChanged")}});
        QSignalSpy passwordChanged(&link, &ServerLink::passwordChanged);
        QTRY_COMPARE(passwordChanged.count(), 1);
        QCOMPARE(told().size(), 1);
        // 网卡变了：再说
        rc::server::WakeInterface wifi = wired;
        wifi.mac = QByteArray::fromHex("aabbccddee02");
        wifi.wifi = true;
        link.setWakeInterfaces({wired, wifi});
        QTRY_COMPARE(told().size(), 2);
        QCOMPARE(told().last().size(), 2);
        // 重新连上：服务器说了账号以后再说
        fake.current->close();
        QTRY_COMPARE(link.state(), ServerLink::State::Offline);
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        FakeServer::send(fake.current, signedIn);
        QTRY_COMPARE(told().size(), 3);
    }

    // 服务器请本机帮忙叫醒账号下的一台、本机请服务器叫醒一台：照原样转
    void passesWakeRequestsAndResults()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);

        QList<rc::server::WakeInterface> asked;
        QString askedBy;
        connect(&link, &ServerLink::wakeRequested, this,
                [&](const QString &, const QString &, const QString &, const QString &by,
                    const QList<rc::server::WakeInterface> &interfaces) {
                    askedBy = by;
                    asked = interfaces;
                });
        // 不合格的网卡（MAC 不是 6 字节）跳过
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("wakeRequest")},
                                        {QStringLiteral("request"), QStringLiteral("r1")},
                                        {QStringLiteral("deviceId"), QStringLiteral("222222222")},
                                        {QStringLiteral("name"), QStringLiteral("客厅电脑")},
                                        {QStringLiteral("by"), QStringLiteral("书房的 Mac")},
                                        {QStringLiteral("interfaces"), QJsonArray{
                                            QJsonObject{{QStringLiteral("mac"), QStringLiteral("aa:bb:cc:dd:ee:01")},
                                                        {QStringLiteral("address"), QStringLiteral("192.168.1.20")},
                                                        {QStringLiteral("prefix"), 24},
                                                        {QStringLiteral("link"), QStringLiteral("wifi")},
                                                        {QStringLiteral("wake"), QStringLiteral("unknown")}},
                                            QJsonObject{{QStringLiteral("mac"), QStringLiteral("aa:bb")},
                                                        {QStringLiteral("address"), QStringLiteral("192.168.1.21")},
                                                        {QStringLiteral("prefix"), 24}}}}});
        QTRY_COMPARE(asked.size(), 1);
        QCOMPARE(askedBy, QStringLiteral("书房的 Mac"));
        QCOMPARE(asked.first().mac, QByteArray::fromHex("aabbccddee01"));
        QVERIFY(asked.first().wifi);
        QCOMPARE(asked.first().wake, rc::server::WakeInterface::Setting::Unknown);

        link.sendWakeAnswer(QStringLiteral("r1"), rc::server::WakeSent);
        QVERIFY(link.requestWake(QStringLiteral("333333333")));
        QTRY_COMPARE(fake.hellos.size(), 3);
        QCOMPARE(fake.hellos.at(1), (QJsonObject{{QStringLiteral("type"), QStringLiteral("wakeAnswer")},
                                                 {QStringLiteral("request"), QStringLiteral("r1")},
                                                 {QStringLiteral("result"), QStringLiteral("sent")}}));
        QCOMPARE(fake.hellos.at(2), (QJsonObject{{QStringLiteral("type"), QStringLiteral("wake")},
                                                 {QStringLiteral("deviceId"), QStringLiteral("333333333")}}));

        QSignalSpy result(&link, &ServerLink::wakeResultReceived);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("wakeResult")},
                                        {QStringLiteral("deviceId"), QStringLiteral("333333333")},
                                        {QStringLiteral("result"), QStringLiteral("sent")},
                                        {QStringLiteral("helper"), QStringLiteral("书房的 Windows")}});
        QTRY_COMPARE(result.count(), 1);
        QCOMPARE(result.at(0), (QVariantList{QStringLiteral("333333333"), QStringLiteral("sent"), QStringLiteral("书房的 Windows")}));
    }

    // 没登录账号：服务器一说就知道了（没有信任的设备），账号本身没变也要通知
    void knowsNoAccount()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake;
        ServerLink link(store, fake.url(), Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Online);
        QVERIFY(!link.trustedKeys());

        QSignalSpy changed(&link, &ServerLink::accountChanged);
        FakeServer::send(fake.current, {{QStringLiteral("type"), QStringLiteral("account")}, {QStringLiteral("account"), QJsonValue()}});
        QTRY_COMPARE(changed.count(), 1);
        QVERIFY(!link.account().signedIn);
        QVERIFY(link.trustedKeys() == QList<QByteArray>{});
    }

    // wss://：系统不信任的证书（自己签的）不连
    void refusesAnUntrustedCertificate()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        FakeServer fake(QStringLiteral("server"));
        QUrl url = fake.url();
        url.setHost(QStringLiteral("127.0.0.1"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("服务器的证书不对")));
        ServerLink link(store, url, Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Offline);
        QTest::qWait(300);
        QVERIFY(fake.hellos.isEmpty());
    }

    void offlineWithoutAServer()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        QUrl url;
        {
            FakeServer gone;
            url = gone.url();
        }
        ServerLink link(store, url, Fast);
        link.start();
        QTRY_COMPARE(link.state(), ServerLink::State::Offline);
        // 一直在重试，状态不在"连接中"和"离线"之间来回跳
        QSignalSpy states(&link, &ServerLink::stateChanged);
        QTest::qWait(300);
        QCOMPARE(states.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestServerLink)
#include "tst_server_link.moc"
