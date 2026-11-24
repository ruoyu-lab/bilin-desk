#include "device_store.h"
#include "test_keys.h"

#include <rc/device_name.h>
#include <rc/json_file.h>
#include <rc/server_protocol.h>

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestDeviceStore : public QObject
{
    Q_OBJECT

private slots:
    void firstRunDefaults()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const auto state = store.state();
        QCOMPARE(state.deviceId.size(), 9);
        QCOMPARE(state.deviceName, rc::defaultDeviceName());
        QCOMPARE(state.password.size(), 6);
        QVERIFY(state.acceptIncoming);
        QVERIFY(state.lanDiscovery);
        QVERIFY(state.unattended);
        QVERIFY(state.connectionStats);
    }

    // 设备密钥只在第一次生成，之后一直是它；公钥是前 65 字节
    void keepsTheDeviceKey()
    {
        QTemporaryDir directory;
        int generated = 0;
        const auto generate = [&generated] {
            ++generated;
            return testDeviceKey();
        };
        QByteArray key;
        {
            DeviceStore store(directory.path(), generate);
            key = store.deviceKey();
            QCOMPARE(key.size(), DeviceStore::DeviceKeySize);
            QCOMPARE(store.publicKey(), key.left(DeviceStore::PublicKeySize));
        }
        DeviceStore again(directory.path(), generate);
        QCOMPARE(again.deviceKey(), key);
        QCOMPARE(generated, 1);
    }

    void keepsIdAndSettingsButNotPassword()
    {
        QTemporaryDir directory;
        rc::protocol::DeviceState before;
        {
            DeviceStore store(directory.path(), testDeviceKey);
            store.setDeviceName(QStringLiteral("  书房的电脑 "));
            store.setAcceptIncoming(false);
            store.setLanDiscovery(false);
            store.setUnattended(false);
            store.setConnectionStats(false);
            before = store.state();
        }
        QCOMPARE(before.deviceName, QStringLiteral("书房的电脑"));

        DeviceStore again(directory.path(), testDeviceKey);
        const auto after = again.state();
        QCOMPARE(after.deviceId, before.deviceId);
        QCOMPARE(after.deviceName, before.deviceName);
        QCOMPARE(after.acceptIncoming, false);
        QCOMPARE(after.lanDiscovery, false);
        QCOMPARE(after.unattended, false);
        QCOMPARE(after.connectionStats, false);
    }

    void keepsTheTokenAndAddsOneWhenMissing()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("identity.json"));
        QVERIFY(rc::writeJsonFile(path, {{QStringLiteral("deviceId"), QStringLiteral("482915307")}}));
        QString token;
        {
            DeviceStore store(directory.path(), testDeviceKey);
            QCOMPARE(store.state().deviceId, QStringLiteral("482915307"));
            token = store.deviceToken();
            QVERIFY(rc::server::isDeviceToken(token));
        }
        DeviceStore again(directory.path(), testDeviceKey);
        QCOMPARE(again.deviceToken(), token);
    }

    // 令牌和设备密钥加密存着；解不开（别的用户、别的电脑上的）就换新的，ID 照旧
    void unreadableSecretsAreReplaced()
    {
        QTemporaryDir directory;
        QVERIFY(rc::writeJsonFile(directory.filePath(QStringLiteral("identity.json")),
                                  {{QStringLiteral("deviceId"), QStringLiteral("482915307")},
                                   {QStringLiteral("secrets"), QStringLiteral("bm90IHNlYWxlZA==")}}));
#ifdef Q_OS_WIN
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("解不开")));
#endif
        DeviceStore store(directory.path(), testDeviceKey);
        QCOMPARE(store.state().deviceId, QStringLiteral("482915307"));
        QVERIFY(rc::server::isDeviceToken(store.deviceToken()));
        QCOMPARE(store.deviceKey().size(), DeviceStore::DeviceKeySize);
    }

    void replacingTheIdKeepsTheToken()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        const auto before = store.state();
        const QString token = store.deviceToken();
        QSignalSpy changed(&store, &DeviceStore::changed);
        store.replaceDeviceId();
        QCOMPARE(changed.count(), 1);
        QVERIFY(store.state().deviceId != before.deviceId);
        QCOMPARE(store.deviceToken(), token);
        DeviceStore again(directory.path(), testDeviceKey);
        QCOMPARE(again.state().deviceId, store.state().deviceId);
    }

    void nameIsCutAt64()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        store.setDeviceName(QString(80, u'名'));
        QCOMPARE(store.state().deviceName.size(), 64);
    }

    void emptyNameMeansDefault()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        store.setDeviceName(QStringLiteral("书房的电脑"));
        store.setDeviceName(QStringLiteral("   "));
        QCOMPARE(store.state().deviceName, rc::defaultDeviceName());
        QVERIFY(!rc::readJsonFile(directory.filePath(QStringLiteral("settings.json"))).contains(u"deviceName"));
    }

    void changesAreAnnouncedOnce()
    {
        QTemporaryDir directory;
        DeviceStore store(directory.path(), testDeviceKey);
        QSignalSpy changed(&store, &DeviceStore::changed);
        store.setAcceptIncoming(true); // 本来就是
        QCOMPARE(changed.count(), 0);
        store.regeneratePassword();
        QCOMPARE(changed.count(), 1);
    }

    void brokenIdentityGetsNewId()
    {
        QTemporaryDir directory;
        QVERIFY(rc::writeJsonFile(directory.filePath(QStringLiteral("identity.json")),
                                  {{QStringLiteral("deviceId"), QStringLiteral("12")}}));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("设备 ID 不对")));
        DeviceStore store(directory.path(), testDeviceKey);
        QCOMPARE(store.state().deviceId.size(), 9);
    }

    // 信任的设备：存下来、重启后还在；同一台再信任一次换掉旧的、排到最前；能改要不要点允许、能取消
    void keepsTrustedDevices()
    {
        QTemporaryDir directory;
        const QByteArray key = QByteArray(1, '\x04') + QByteArray(64, '\x11');
        const QString base64 = QString::fromLatin1(key.toBase64());
        {
            DeviceStore store(directory.path(), testDeviceKey);
            QSignalSpy changed(&store, &DeviceStore::trustChanged);
            store.addTrusted({QStringLiteral("482915307"), QStringLiteral("小明的电脑"), QStringLiteral("Windows"), base64, true, 1});
            store.addTrusted({QStringLiteral("731204586"), QStringLiteral("办公室"), QStringLiteral("macOS"), base64, false, 2});
            store.addTrusted({QStringLiteral("482915307"), QStringLiteral("小明的新名字"), QStringLiteral("Windows"), base64, true, 3});
            QCOMPARE(changed.count(), 3);
            store.setTrustAsk(QStringLiteral("482915307"), false);
            store.setTrustAsk(QStringLiteral("482915307"), false); // 没变
            QCOMPARE(changed.count(), 4);
            store.rememberTrust(QStringLiteral("100049306"), key);
            store.rememberTrust(QStringLiteral("100049306"), key); // 没变
            QCOMPARE(changed.count(), 5);
        }
        DeviceStore store(directory.path(), testDeviceKey);
        const rc::protocol::TrustState trust = store.trust();
        QCOMPARE(trust.trusted.size(), 2);
        QCOMPARE(trust.trusted.first().name, QStringLiteral("小明的新名字"));
        QVERIFY(!trust.trusted.first().ask);
        QCOMPARE(trust.trusted.last().deviceId, QStringLiteral("731204586"));
        QCOMPARE(store.trusted(QStringLiteral("731204586"))->publicKey, base64);
        QCOMPARE(trust.trustedBy.size(), 1);
        store.untrust(QStringLiteral("731204586"));
        store.forgetTrust(QStringLiteral("100049306"));
        QVERIFY(!store.trusted(QStringLiteral("731204586")));
        QVERIFY(store.trust().trustedBy.isEmpty());
    }

    // 公钥、设备 ID 不对的不收：不管是界面发来的，还是文件里的
    void refusesBadTrust()
    {
        QTemporaryDir directory;
        const QString good = QString::fromLatin1((QByteArray(1, '\x04') + QByteArray(64, '\x22')).toBase64());
        const QString shortKey = QString::fromLatin1(QByteArray(10, '\x04').toBase64());
        QVERIFY(rc::writeJsonFile(
            directory.filePath(QStringLiteral("trust.json")),
            rc::protocol::TrustState{{{QStringLiteral("482915307"), {}, {}, good, true, 0},
                                      {QStringLiteral("731204586"), {}, {}, shortKey, true, 0},
                                      {QStringLiteral("12"), {}, {}, good, true, 0}},
                                     {{QStringLiteral("100049306"), shortKey}}}
                .toJson()));
        DeviceStore store(directory.path(), testDeviceKey);
        QCOMPARE(store.trust().trusted.size(), 1);
        QVERIFY(store.trust().trustedBy.isEmpty());
        store.addTrusted({QStringLiteral("731204586"), {}, {}, shortKey, true, 0});
        store.rememberTrust(QStringLiteral("731204586"), QByteArray(65, '\x01'));
        QCOMPARE(store.trust().trusted.size(), 1);
        QVERIFY(store.trust().trustedBy.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDeviceStore)
#include "tst_device_store.moc"
