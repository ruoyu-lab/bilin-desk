#include "files/file_transfer_list.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace transfer = rc::transfer;

namespace {

// 界面上的列表本身（ViewerFiles、IncomingFiles 都是它）：收下的记下来，别的命令不用
class Files : public FileTransferList
{
public:
    Files()
        : FileTransferList(nullptr)
    {
    }
    using FileTransferList::setTransfers;

    void reject(int) override {}
    void cancel(int) override {}
    void retry(int) override {}
    void remove(int) override {}

    QList<std::pair<int, QString>> accepted;

protected:
    void acceptInto(int id, const QString &directory) override { accepted.append({id, directory}); }
};

rc::Transfer offer(int id, qint64 totalBytes)
{
    rc::Transfer t;
    t.id = id;
    t.name = QStringLiteral("视频.mov");
    t.entries = 1;
    t.files = 1;
    t.totalBytes = totalBytes;
    t.status = transfer::Offered;
    return t;
}

rc::Transfer disconnected(int id, bool sending, bool resumable)
{
    rc::Transfer t;
    t.id = id;
    t.sending = sending;
    t.name = QStringLiteral("报告.pdf");
    t.entries = 3;
    t.status = transfer::Failed;
    t.reason = transfer::Disconnected;
    t.resumable = resumable;
    return t;
}

QVariantMap item(const QVariantList &list, int id)
{
    for (const QVariant &value : list) {
        if (value.toMap().value(QStringLiteral("id")).toInt() == id)
            return value.toMap();
    }
    return {};
}

} // namespace

class TestFileTransferList : public QObject
{
    Q_OBJECT

private slots:
    // 等本机决定的在 offers 里，别的在 transfers 里；进度变了不动 offers
    void offersAreApart()
    {
        Files files;
        QSignalSpy offers(&files, &FileTransferList::offersChanged);
        rc::Transfer sending = disconnected(2, true, false);
        sending.status = transfer::Running;
        files.setTransfers({offer(1, 10), sending});
        QCOMPARE(files.offers().size(), 1);
        QCOMPARE(files.transfers().size(), 1);
        QCOMPARE(offers.count(), 1);
        sending.doneBytes = 5;
        files.setTransfers({offer(1, 10), sending});
        QCOMPARE(offers.count(), 1);
        QCOMPARE(item(files.transfers(), 2).value(QStringLiteral("name")).toString(), QStringLiteral("报告.pdf 等 3 项"));
    }

    // 断了的：留着接着传的说怎么接着传
    void disconnectedSaysHowToGoOn()
    {
        Files files;
        files.setTransfers({disconnected(1, true, true), disconnected(2, false, true), disconnected(3, true, false),
                            disconnected(4, false, false)});
        const QVariantList list = files.transfers();
        QCOMPARE(item(list, 1).value(QStringLiteral("message")).toString(), QStringLiteral("没发完，重试时接着发"));
        QCOMPARE(item(list, 2).value(QStringLiteral("message")).toString(), QStringLiteral("没收完，对方再发时接着收"));
        QCOMPARE(item(list, 3).value(QStringLiteral("message")).toString(), QStringLiteral("连接断了，没发完"));
        QCOMPARE(item(list, 4).value(QStringLiteral("message")).toString(), QStringLiteral("连接断了，没收完"));
        QVERIFY(item(list, 1).value(QStringLiteral("resumable")).toBool());
        QVERIFY(!item(list, 3).value(QStringLiteral("resumable")).toBool());
    }

    void rejectedSaysWhy()
    {
        Files files;
        rc::Transfer timedOut = disconnected(1, true, false);
        timedOut.status = transfer::Rejected;
        timedOut.reason = transfer::TimedOut;
        rc::Transfer unattended = timedOut;
        unattended.id = 2;
        unattended.reason = transfer::Unattended;
        files.setTransfers({timedOut, unattended});
        QCOMPARE(item(files.transfers(), 1).value(QStringLiteral("message")).toString(), QStringLiteral("对方一直没有接收"));
        QCOMPARE(item(files.transfers(), 2).value(QStringLiteral("message")).toString(),
                 QStringLiteral("对方电脑前没有人能接收"));
    }

    void acceptsIntoTheChosenFolder()
    {
        QTemporaryDir chosen;
        Files files;
        files.setTransfers({offer(1, 10)});
        QCOMPARE(files.accept(1, QUrl::fromLocalFile(chosen.path())), QString());
        QCOMPARE(files.accepted, (QList<std::pair<int, QString>>{{1, chosen.path()}}));
    }

    // 文件夹不在了、磁盘不够：说明，不收
    void refusesWhatCannotBeStored()
    {
        QTemporaryDir chosen;
        Files files;
        files.setTransfers({offer(1, 10), offer(2, qint64(1) << 60)});
        const QString gone = QDir(chosen.path()).filePath(QStringLiteral("不在了"));
        QVERIFY(files.accept(1, QUrl::fromLocalFile(gone)).contains(QStringLiteral("不在了")));
        QVERIFY(files.accept(2, QUrl::fromLocalFile(chosen.path())).startsWith(QStringLiteral("存不下")));
        QVERIFY(files.accepted.isEmpty());
    }

    // 接着上次没传完的：存回上次的文件夹（半截文件在那里）；它不在了就存到选的文件夹，从头收
    void resumedGoesBackToItsFolder()
    {
        QTemporaryDir chosen;
        QTemporaryDir earlier;
        Files files;
        rc::Transfer resumed = offer(1, 10);
        resumed.resumed = true;
        resumed.directory = earlier.path();
        resumed.doneBytes = 4;
        rc::Transfer moved = resumed;
        moved.id = 2;
        moved.directory = QDir(earlier.path()).filePath(QStringLiteral("删掉了"));
        files.setTransfers({resumed, moved});
        QVERIFY(item(files.offers(), 1).value(QStringLiteral("resumed")).toBool());
        QCOMPARE(files.accept(1, QUrl::fromLocalFile(chosen.path())), QString());
        QCOMPARE(files.accept(2, QUrl::fromLocalFile(chosen.path())), QString());
        QCOMPARE(files.accepted, (QList<std::pair<int, QString>>{{1, earlier.path()}, {2, chosen.path()}}));
    }
};

QTEST_GUILESS_MAIN(TestFileTransferList)
#include "tst_file_transfer_list.moc"
