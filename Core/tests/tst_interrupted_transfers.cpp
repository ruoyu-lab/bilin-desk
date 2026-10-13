#include <rc/interrupted_transfers.h>
#include <rc/json_file.h>

#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

namespace {

const QDateTime Now = QDateTime(QDate(2026, 10, 11), QTime(9, 0), QTimeZone::UTC);

rc::InterruptedTransfer sending(char token, const QString &peer)
{
    rc::InterruptedTransfer t;
    t.token = QByteArray(16, token);
    t.peer = peer;
    t.sending = true;
    t.name = QStringLiteral("视频.mov");
    t.entries = 1;
    t.totalBytes = 4'000'000'000;
    t.doneBytes = 1'000'000'000;
    t.paths = {QStringLiteral("/Users/me/视频.mov")};
    t.at = Now;
    return t;
}

rc::InterruptedTransfer receiving(char token, const QString &peer)
{
    rc::InterruptedTransfer t;
    t.token = QByteArray(16, token);
    t.peer = peer;
    t.name = QStringLiteral("photos");
    t.entries = 1;
    t.files = 25;
    t.totalBytes = 340'000;
    t.directory = QStringLiteral("/Users/me/Downloads");
    t.at = Now;
    return t;
}

} // namespace

class TestInterruptedTransfers : public QObject
{
    Q_OBJECT

private slots:
    void keepsThemAcrossRuns()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath(QStringLiteral("transfers.json"));
        {
            rc::InterruptedTransfers transfers(file, Now);
            transfers.put(sending('a', QStringLiteral("482915307")));
            transfers.put(receiving('b', QStringLiteral("482915307")));
            transfers.put(receiving('c', QStringLiteral("192.168.1.20")));
        }
        rc::InterruptedTransfers again(file, Now);
        const QList<rc::InterruptedTransfer> mine = again.forPeer(QStringLiteral("482915307"));
        QCOMPARE(mine.size(), 2);
        QCOMPARE(mine.at(0), sending('a', QStringLiteral("482915307")));
        QCOMPARE(mine.at(1), receiving('b', QStringLiteral("482915307")));
        QCOMPARE(again.forPeer(QStringLiteral("192.168.1.20")).size(), 1);
        QVERIFY(again.find(QByteArray(16, 'c')));
        QVERIFY(!again.find(QByteArray(16, 'd')));
    }

    void putReplacesTheSameToken()
    {
        QTemporaryDir directory;
        rc::InterruptedTransfers transfers(directory.filePath(QStringLiteral("transfers.json")), Now);
        rc::InterruptedTransfer t = sending('a', QStringLiteral("482915307"));
        transfers.put(t);
        t.doneBytes = 2'000'000'000;
        transfers.put(t);
        QCOMPARE(transfers.forPeer(t.peer), QList<rc::InterruptedTransfer>{t});
        transfers.remove(t.token);
        QVERIFY(transfers.forPeer(t.peer).isEmpty());
    }

    // 不要了：发的直接去掉；收的等着删掉半截文件，不再算没传完的
    void discarding()
    {
        QTemporaryDir directory;
        rc::InterruptedTransfers transfers(directory.filePath(QStringLiteral("transfers.json")), Now);
        transfers.put(sending('a', QStringLiteral("482915307")));
        transfers.put(receiving('b', QStringLiteral("482915307")));
        transfers.discard(QByteArray(16, 'a'));
        transfers.discard(QByteArray(16, 'b'));
        QVERIFY(!transfers.find(QByteArray(16, 'a')));
        QVERIFY(transfers.forPeer(QStringLiteral("482915307")).isEmpty());
        const QList<rc::InterruptedTransfer> discarded = transfers.discarded();
        QCOMPARE(discarded.size(), 1);
        QCOMPARE(discarded.first().token, QByteArray(16, 'b'));
        transfers.remove(QByteArray(16, 'b'));
        QVERIFY(transfers.discarded().isEmpty());
    }

    // 放了 30 天的：发的去掉，收的当作不要了
    void oldOnesGo()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath(QStringLiteral("transfers.json"));
        {
            rc::InterruptedTransfers transfers(file, Now);
            transfers.put(sending('a', QStringLiteral("482915307")));
            transfers.put(receiving('b', QStringLiteral("482915307")));
        }
        rc::InterruptedTransfers later(file, Now.addDays(rc::InterruptedTransfers::KeepDays + 1));
        QVERIFY(!later.find(QByteArray(16, 'a')));
        QVERIFY(later.forPeer(QStringLiteral("482915307")).isEmpty());
        QCOMPARE(later.discarded().size(), 1);
        // 存回去了
        rc::InterruptedTransfers again(file, Now);
        QCOMPARE(again.discarded().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestInterruptedTransfers)
#include "tst_interrupted_transfers.moc"
