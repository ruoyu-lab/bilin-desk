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
};

QTEST_GUILESS_MAIN(TestDeviceStore)
#include "tst_device_store.moc"
