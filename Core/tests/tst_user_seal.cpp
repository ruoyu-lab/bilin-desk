#include <rc/user_seal.h>

#include <QTest>

class TestUserSeal : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        const QByteArray secret("令牌和设备私钥", -1);
        const std::optional<QByteArray> opened = rc::unsealForUser(rc::sealForUser(secret));
        QVERIFY(opened);
        QCOMPARE(*opened, secret);
    }

#ifdef Q_OS_WIN
    // 存下来的看不出原文；改过一个字节、不是 DPAPI 加密的都解不开
    void sealedIsUnreadable()
    {
        const QByteArray secret("482915307 的设备令牌");
        QByteArray sealed = rc::sealForUser(secret);
        QVERIFY(!sealed.contains(secret));
        sealed[sealed.size() - 1] = char(sealed.at(sealed.size() - 1) ^ 0x01);
        QVERIFY(!rc::unsealForUser(sealed));
        QVERIFY(!rc::unsealForUser(secret));
        QVERIFY(!rc::unsealForUser({}));
    }
#endif
};

QTEST_GUILESS_MAIN(TestUserSeal)
#include "tst_user_seal.moc"
