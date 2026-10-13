#include <rc/device_id.h>

#include <QTest>

class TestDeviceId : public QObject
{
    Q_OBJECT

private slots:
    void generatedIsNineDigitsWithoutLeadingZero()
    {
        for (int i = 0; i < 200; ++i) {
            const rc::DeviceId id = rc::DeviceId::generate();
            QVERIFY(id.isValid());
            QCOMPARE(id.digits().size(), 9);
            QVERIFY(id.digits().front() != u'0');
            QCOMPARE(rc::DeviceId::parse(id.digits()), id);
        }
    }

    void displayGroupsByThree()
    {
        QCOMPARE(rc::DeviceId::parse(u"482915307").display(), QStringLiteral("482 915 307"));
        QCOMPARE(rc::DeviceId().display(), QString());
    }

    void parseIgnoresSpacesAndDashes()
    {
        QCOMPARE(rc::DeviceId::parse(u" 482 915-307 ").digits(), QStringLiteral("482915307"));
    }

    void parseRejectsOtherInput_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("short") << QStringLiteral("48291530");
        QTest::newRow("long") << QStringLiteral("4829153071");
        QTest::newRow("leading zero") << QStringLiteral("082915307");
        QTest::newRow("letter") << QStringLiteral("48291530a");
        QTest::newRow("full-width digit") << QStringLiteral("48291530７");
    }

    void parseRejectsOtherInput()
    {
        QFETCH(QString, text);
        QVERIFY(!rc::DeviceId::parse(text).isValid());
    }
};

QTEST_GUILESS_MAIN(TestDeviceId)
#include "tst_device_id.moc"
