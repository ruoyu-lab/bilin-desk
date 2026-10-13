#include <rc/ipc/endpoint.h>
#include <rc/paths.h>

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

class TestPaths : public QObject
{
    Q_OBJECT

private slots:
    void cleanup() { qunsetenv(rc::StateDirectoryVariable); }

    // 设了 RC_STATE_DIR：数据目录是它（没有就建好），本机连接的名字也和平常的、别的目录的都不一样
    void stateDirectoryCanBeSwapped()
    {
        const QString usual = rc::ipc::serviceEndpoint();
        QTemporaryDir first;
        QTemporaryDir second;
        qputenv(rc::StateDirectoryVariable, first.filePath(QStringLiteral("a")).toUtf8());
        QCOMPARE(rc::stateDirectory(), first.filePath(QStringLiteral("a")));
        QVERIFY(QDir(first.filePath(QStringLiteral("a"))).exists());
        const QString swapped = rc::ipc::serviceEndpoint();
        QVERIFY(swapped != usual);

        qputenv(rc::StateDirectoryVariable, second.filePath(QStringLiteral("a")).toUtf8());
        QVERIFY(rc::ipc::serviceEndpoint() != swapped);
        qunsetenv(rc::StateDirectoryVariable);
        QCOMPARE(rc::ipc::serviceEndpoint(), usual);
    }
};

QTEST_GUILESS_MAIN(TestPaths)
#include "tst_paths.moc"
