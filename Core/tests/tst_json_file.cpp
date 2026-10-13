#include <rc/json_file.h>

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

class TestJsonFile : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings.json"));
        const QJsonObject object{{QStringLiteral("deviceName"), QStringLiteral("书房的电脑")},
                                 {QStringLiteral("acceptIncoming"), false}};
        QVERIFY(rc::writeJsonFile(path, object));
        QCOMPARE(rc::readJsonFile(path), object);
    }

    void missingFileIsEmpty()
    {
        QTemporaryDir directory;
        QVERIFY(rc::readJsonFile(directory.filePath(QStringLiteral("missing.json"))).isEmpty());
    }

    void brokenFileIsEmpty()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("broken.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[1, 2");
        file.close();
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("内容不对")));
        QVERIFY(rc::readJsonFile(path).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestJsonFile)
#include "tst_json_file.moc"
