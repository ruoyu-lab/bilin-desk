#include <rc/transfer_list.h>

#include <QTest>

namespace transfer = rc::transfer;

class TestTransferList : public QObject
{
    Q_OBJECT

private slots:
    void sendGoesThroughItsStates()
    {
        rc::TransferList list;
        const int id = list.addSending(2, {QStringLiteral("/Users/me/报告.pdf"), QStringLiteral("/Users/me/photos")});
        const rc::Transfer *t = list.find(id);
        QVERIFY(t);
        QVERIFY(t->sending);
        QCOMPARE(t->viewer, 2);
        QCOMPARE(t->name, QStringLiteral("报告.pdf"));
        QCOMPARE(t->entries, 2);
        QCOMPARE(t->status, transfer::Preparing);
        QCOMPARE(list.paths(id), (QStringList{QStringLiteral("/Users/me/报告.pdf"), QStringLiteral("/Users/me/photos")}));

        list.started(id, 7, {});
        QCOMPARE(list.sdkId(id), quint64(7));
        QCOMPARE(list.findSdk(7), list.find(id));
        list.setStatus(7, 2, transfer::Offered);
        QCOMPARE(list.find(id)->status, transfer::Offered);
        list.setStatus(7, 2, transfer::Running);
        list.setProgress(7, 2, 300, 1000, 150);
        t = list.find(id);
        QCOMPARE(t->doneBytes, qint64(300));
        QCOMPARE(t->totalBytes, qint64(1000));
        QCOMPARE(t->bytesPerSecond, qint64(150));

        list.finish(7, 2, transfer::Completed, {}, 0, 1000, {}, false);
        t = list.find(id);
        QCOMPARE(t->status, transfer::Completed);
        QVERIFY(t->finished());
        QCOMPARE(t->doneBytes, qint64(1000));
        QCOMPARE(t->bytesPerSecond, qint64(0));
        // 结束了就不在 SDK 里：之后同一个 SDK 编号的事件不算它的
        QCOMPARE(list.sdkId(id), quint64(0));
        QVERIFY(!list.findSdk(7));
        list.setStatus(7, 2, transfer::Running);
        QCOMPARE(list.find(id)->status, transfer::Completed);
    }

    void rootHasItsPathAsName()
    {
        rc::TransferList list;
        const int id = list.addSending(0, {QStringLiteral("/")});
        QCOMPARE(list.find(id)->name, QStringLiteral("/"));
    }

    void offerSaysWhatComes()
    {
        rc::TransferList list;
        list.offered(4, 1,
                     {{QStringLiteral("photos"), false},
                      {QStringLiteral("photos/a.jpg"), true},
                      {QStringLiteral("photos/sub"), false},
                      {QStringLiteral("photos/sub/b.txt"), true},
                      {QStringLiteral("notes.txt"), true}},
                     12345, QByteArray(16, '\x07'));
        QCOMPARE(list.offers().size(), 1);
        const int id = list.offers().first();
        const rc::Transfer *t = list.find(id);
        QVERIFY(!t->sending);
        QCOMPARE(t->viewer, 1);
        QCOMPARE(t->name, QStringLiteral("photos"));
        QCOMPARE(t->entries, 2);
        QCOMPARE(t->files, 3);
        QCOMPARE(t->totalBytes, qint64(12345));
        QCOMPARE(t->status, transfer::Offered);
        QCOMPARE(list.sdkId(id), quint64(4));
        QCOMPARE(list.token(id), QByteArray(16, '\x07'));
        QCOMPARE(list.findToken(QByteArray(16, '\x07')), t);

        list.accepted(id, QStringLiteral("/Users/me/Downloads"));
        QVERIFY(list.offers().isEmpty());
        QCOMPARE(list.find(id)->status, transfer::Preparing);
        QCOMPARE(list.find(id)->directory, QStringLiteral("/Users/me/Downloads"));

        list.finish(4, 1, transfer::Completed, transfer::Partial, 1, 12345, QStringLiteral("/Users/me/Downloads/photos (1)"), false);
        QCOMPARE(list.find(id)->reason, transfer::Partial);
        QCOMPARE(list.find(id)->missed, 1);
        QCOMPARE(list.find(id)->savedPath, QStringLiteral("/Users/me/Downloads/photos (1)"));
    }

    // 分享端：对方重连回来是新的编号，他的传输跟着走
    void viewerFollowsTheSdk()
    {
        rc::TransferList list;
        const int id = list.addSending(1, {QStringLiteral("/tmp/a")});
        list.started(id, 3, {});
        list.setStatus(3, 1, transfer::Waiting);
        list.setStatus(3, 5, transfer::Running);
        QCOMPARE(list.find(id)->viewer, 5);
        list.setProgress(3, 6, 1, 2, 3);
        QCOMPARE(list.find(id)->viewer, 6);
    }

    void failedSendCanBeRetried()
    {
        rc::TransferList list;
        const int first = list.addSending(0, {QStringLiteral("/tmp/a")});
        const int second = list.addSending(0, {QStringLiteral("/tmp/b")});
        list.failed(first, transfer::Unsupported);
        QCOMPARE(list.find(first)->status, transfer::Failed);
        QCOMPARE(list.find(first)->reason, transfer::Unsupported);

        QVERIFY(list.restart(first));
        const rc::Transfer *t = list.find(first);
        QCOMPARE(t->status, transfer::Preparing);
        QVERIFY(t->reason.isEmpty());
        QCOMPARE(list.paths(first), QStringList{QStringLiteral("/tmp/a")});
        // 重试的是最新的一次，排到最后
        QCOMPARE(list.all().last().id, first);
        QCOMPARE(list.all().first().id, second);
        // 没结束的、完成了的、收的都不能重试
        QVERIFY(!list.restart(first));
        list.started(first, 9, {});
        list.finish(9, 0, transfer::Completed, {}, 0, 1, {}, false);
        QVERIFY(!list.restart(first));
        list.offered(10, 0, {{QStringLiteral("x"), true}}, 1, {});
        const int received = list.offers().first();
        list.finish(10, 0, transfer::Rejected, transfer::TimedOut, 0, 0, {}, false);
        QVERIFY(!list.restart(received));
    }

    void detachingFailsWhatWasGoing()
    {
        rc::TransferList list;
        const int going = list.addSending(0, {QStringLiteral("/tmp/a")});
        list.started(going, 1, QByteArray(16, '\x01'));
        list.setStatus(1, 0, transfer::Running);
        const int done = list.addSending(0, {QStringLiteral("/tmp/b")});
        list.started(done, 2, {});
        list.finish(2, 0, transfer::Completed, {}, 0, 5, {}, false);
        list.offered(3, 0, {{QStringLiteral("c"), true}}, 1, {});

        list.detached();
        // 等着决定的不要了，没结束的算失败（连接断了），结束了的不变
        QVERIFY(list.offers().isEmpty());
        QCOMPARE(list.all().size(), 2);
        QCOMPARE(list.find(going)->status, transfer::Failed);
        QCOMPARE(list.find(going)->reason, transfer::Disconnected);
        // 已经开始传的（有 token）下次接着传
        QVERIFY(list.find(going)->resumable);
        QCOMPARE(list.find(done)->status, transfer::Completed);
        QCOMPARE(list.sdkId(going), quint64(0));
        QVERIFY(!list.findSdk(1));
    }

    void removesAndClears()
    {
        rc::TransferList list;
        const int completed = list.addSending(0, {QStringLiteral("/tmp/a")});
        list.started(completed, 1, {});
        list.finish(1, 0, transfer::Completed, {}, 0, 1, {}, false);
        const int failed = list.addSending(0, {QStringLiteral("/tmp/b")});
        list.failed(failed, transfer::Disconnected);
        const int going = list.addSending(0, {QStringLiteral("/tmp/c")});
        list.started(going, 2, {});

        list.clearCompleted();
        QVERIFY(!list.find(completed));
        QVERIFY(list.find(failed));
        list.remove(failed);
        QVERIFY(!list.find(failed));
        QVERIFY(list.find(going));
        list.clear();
        QVERIFY(list.all().isEmpty());
    }

    void keepsTheLatestFinished()
    {
        rc::TransferList list;
        const int going = list.addSending(0, {QStringLiteral("/tmp/going")});
        list.started(going, 1000, {});
        QList<int> ids;
        for (int i = 0; i < rc::TransferList::KeptFinished + 3; ++i) {
            ids.append(list.addSending(0, {QStringLiteral("/tmp/%1").arg(i)}));
            list.failed(ids.last(), transfer::Disconnected);
        }
        QCOMPARE(list.all().size(), rc::TransferList::KeptFinished + 1);
        QVERIFY(list.find(going));
        QVERIFY(!list.find(ids[2]));
        QVERIFY(list.find(ids[3]));
        QVERIFY(list.find(ids.last()));
    }

    void idsAreNotReused()
    {
        rc::TransferList list;
        const int first = list.addSending(0, {QStringLiteral("/tmp/a")});
        list.remove(first);
        list.offered(1, 0, {{QStringLiteral("b"), true}}, 1, {});
        QVERIFY(list.offers().first() != first);
    }

    // 上次没传完的：失败了（连接断了），留着接着传；重试时 token 不变
    void interruptedComesBack()
    {
        rc::TransferList list;
        rc::InterruptedTransfer sending;
        sending.token = QByteArray(16, '\x02');
        sending.peer = QStringLiteral("482915307");
        sending.sending = true;
        sending.name = QStringLiteral("视频.mov");
        sending.entries = 1;
        sending.totalBytes = 4'000'000'000;
        sending.doneBytes = 1'000'000'000;
        sending.paths = {QStringLiteral("/Users/me/视频.mov")};
        const int id = list.addInterrupted(3, sending);
        const rc::Transfer *t = list.find(id);
        QCOMPARE(t->viewer, 3);
        QCOMPARE(t->status, transfer::Failed);
        QCOMPARE(t->reason, transfer::Disconnected);
        QVERIFY(t->resumable);
        QCOMPARE(t->doneBytes, qint64(1'000'000'000));
        QCOMPARE(list.paths(id), sending.paths);
        QCOMPARE(list.findToken(sending.token), t);

        list.setViewer(id, 5);
        QCOMPARE(list.find(id)->viewer, 5);
        QVERIFY(list.restart(id));
        QVERIFY(!list.find(id)->resumable);
        QCOMPARE(list.token(id), sending.token);

        list.started(id, 8, sending.token);
        list.finish(8, 5, transfer::Failed, transfer::Disconnected, 0, 2'000'000'000, {}, true);
        QVERIFY(list.find(id)->resumable);
        QVERIFY(!list.findToken({}));
    }

    // 对方接着发上次没传完的：存回上次的文件夹
    void resumedOfferGoesBack()
    {
        rc::TransferList list;
        const QByteArray token(16, '\x03');
        const int id = list.offered(6, 0, {{QStringLiteral("视频.mov"), true}}, 4'000'000'000, token);
        list.resuming(id, QStringLiteral("/Users/me/Desktop"), 1'000'000'000);
        const rc::Transfer *t = list.find(id);
        QVERIFY(t->resumed);
        QCOMPARE(t->directory, QStringLiteral("/Users/me/Desktop"));
        QCOMPARE(t->doneBytes, qint64(1'000'000'000));
        QCOMPARE(list.offers(), QList<int>{id});
    }

    void topicRoundTrips()
    {
        rc::Transfer sent;
        sent.id = 3;
        sent.viewer = 2;
        sent.sending = true;
        sent.name = QStringLiteral("报告.pdf");
        sent.entries = 2;
        sent.totalBytes = 5'000'000'000;
        sent.doneBytes = 1'234'567'890;
        sent.bytesPerSecond = 12'000'000;
        sent.status = transfer::Running;
        rc::Transfer received;
        received.id = 4;
        received.name = QStringLiteral("photos");
        received.entries = 1;
        received.files = 25;
        received.totalBytes = 340;
        received.doneBytes = 340;
        received.status = transfer::Completed;
        received.reason = transfer::Partial;
        received.missed = 1;
        received.directory = QStringLiteral("/Users/me/Downloads");
        received.savedPath = QStringLiteral("/Users/me/Downloads/photos (1)");
        received.resumed = true;
        rc::Transfer interrupted = sent;
        interrupted.id = 5;
        interrupted.status = transfer::Failed;
        interrupted.reason = transfer::Disconnected;
        interrupted.resumable = true;
        const QList<rc::Transfer> transfers{sent, received, interrupted};
        QCOMPARE(rc::transfersFromJson(rc::transfersToJson(transfers)), transfers);
    }
};

QTEST_GUILESS_MAIN(TestTransferList)
#include "tst_transfer_list.moc"
