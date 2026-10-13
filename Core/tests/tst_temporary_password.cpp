#include <rc/temporary_password.h>

#include <QSet>
#include <QTest>

class TestTemporaryPassword : public QObject
{
    Q_OBJECT

private slots:
    void lengthAndAlphabet()
    {
        const QString alphabet = QString::fromLatin1(rc::TemporaryPasswordAlphabet);
        for (const char confusing : {'0', 'o', '1', 'l', 'i'})
            QVERIFY(!alphabet.contains(QLatin1Char(confusing)));
        for (int i = 0; i < 200; ++i) {
            const QString password = rc::generateTemporaryPassword();
            QCOMPARE(password.size(), rc::TemporaryPasswordLength);
            for (const QChar c : password)
                QVERIFY2(alphabet.contains(c), qUtf8Printable(password));
        }
    }

    void differsEachTime()
    {
        QSet<QString> seen;
        for (int i = 0; i < 100; ++i)
            seen.insert(rc::generateTemporaryPassword());
        QVERIFY(seen.size() > 95);
    }
};

QTEST_GUILESS_MAIN(TestTemporaryPassword)
#include "tst_temporary_password.moc"
