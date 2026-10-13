#include <rc/machine.h>
#include <rc/server_protocol.h>

#include <QSet>
#include <QRegularExpression>
#include <QTest>

class TestServerProtocol : public QObject
{
    Q_OBJECT

private slots:
    void tokenIs32BytesOfBase64Url()
    {
        QSet<QString> seen;
        for (int i = 0; i < 100; ++i) {
            const QString token = rc::server::generateDeviceToken();
            QCOMPARE(token.size(), 43);
            QVERIFY2(rc::server::isDeviceToken(token), qUtf8Printable(token));
            QCOMPARE(QByteArray::fromBase64(token.toLatin1(), QByteArray::Base64UrlEncoding).size(), 32);
            seen.insert(token);
        }
        QCOMPARE(seen.size(), 100);
    }

    void rejectsOtherTokens_data()
    {
        QTest::addColumn<QString>("token");
        QTest::newRow("empty") << QString();
        QTest::newRow("short") << QString(42, u'A');
        QTest::newRow("padded") << QString(42, u'A') + u'=';
        QTest::newRow("plus") << QString(42, u'A') + u'+';
    }

    void rejectsOtherTokens()
    {
        QFETCH(QString, token);
        QVERIFY(!rc::server::isDeviceToken(token));
    }

    void helloCarriesTheDevice()
    {
        const QJsonObject hello =
            rc::server::hello(QStringLiteral("482915307"), QStringLiteral("t"), QByteArray(65, 4), QString(70, u'名'),
                              QStringLiteral("0.1"), rc::machineHash());
        QCOMPARE(hello.value(u"type").toString(), QStringLiteral("hello"));
        QCOMPARE(hello.value(u"deviceId").toString(), QStringLiteral("482915307"));
        QCOMPARE(hello.value(u"name").toString().size(), rc::server::MaxNameLength);
        QCOMPARE(hello.value(u"platform").toString(), rc::server::platformName());
        QVERIFY(!rc::server::platformName().isEmpty());
        QCOMPARE(hello.value(u"machine").toString(), rc::machineHash());
    }

    // 硬件编号的哈希：sha256 的十六进制（服务器只收这样的），同一台电脑每次一样，不是编号本身
    void machineHashIsStable()
    {
        const QString hash = rc::machineHash();
        QCOMPARE(hash.size(), 64);
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$")).match(hash).hasMatch());
        QCOMPARE(rc::machineHash(), hash);
        QVERIFY(!hash.contains(rc::machineId(), Qt::CaseInsensitive));
    }

    // 和 Server/src/protocol.ts 的 setNickname、changePassword、deleteAccount 对得上
    void accountChangesMatchTheServer()
    {
        QCOMPARE(rc::server::setNickname(QStringLiteral("新昵称")),
                 (QJsonObject{{QStringLiteral("type"), QStringLiteral("setNickname")},
                              {QStringLiteral("nickname"), QStringLiteral("新昵称")}}));
        QCOMPARE(rc::server::changePassword(QStringLiteral("old"), QStringLiteral("password2")),
                 (QJsonObject{{QStringLiteral("type"), QStringLiteral("changePassword")},
                              {QStringLiteral("oldPassword"), QStringLiteral("old")},
                              {QStringLiteral("newPassword"), QStringLiteral("password2")}}));
        QCOMPARE(rc::server::deleteAccount(QStringLiteral("password1")),
                 (QJsonObject{{QStringLiteral("type"), QStringLiteral("deleteAccount")},
                              {QStringLiteral("password"), QStringLiteral("password1")}}));
    }
};

QTEST_GUILESS_MAIN(TestServerProtocol)
#include "tst_server_protocol.moc"
