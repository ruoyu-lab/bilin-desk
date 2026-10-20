#include "connection_history.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace protocol = rc::protocol;

namespace {

protocol::SharePeer peer(const QString &deviceId, bool control = false)
{
    return {1, deviceId, QStringLiteral("书房的 Mac"), QStringLiteral("macOS"), control};
}

} // namespace

class TestConnectionHistory : public QObject
{
    Q_OBJECT

private slots:
    // 一次连接一条：连进来时记下，走了补上时间；新的在前
    void recordsAConnectionFromStartToEnd()
    {
        QTemporaryDir directory;
        qint64 now = 1000;
        ConnectionHistory history(directory.filePath(QStringLiteral("history.json")), [&now] { return now; });
        QSignalSpy changed(&history, &ConnectionHistory::changed);
        history.connected(peer(QStringLiteral("111111111"), true), QStringLiteral("s1"));
        now = 61000;
        history.wrongPin();
        now = 125000;
        history.gone(QStringLiteral("111111111"), QStringLiteral("s1"));
        QCOMPARE(changed.count(), 3);

        const auto entries = history.state().entries;
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).kind, QStringLiteral("wrongPin"));
        QCOMPARE(entries.at(1).kind, QStringLiteral("connection"));
        QCOMPARE(entries.at(1).deviceId, QStringLiteral("111111111"));
        QVERIFY(entries.at(1).control);
        QCOMPARE(entries.at(1).at, 1000);
        QCOMPARE(entries.at(1).endedAt, 125000);
    }

    void recordsRefusalsAndFiles()
    {
        QTemporaryDir directory;
        ConnectionHistory history(directory.filePath(QStringLiteral("history.json")));
        history.refused(peer(QStringLiteral("222222222")), QStringLiteral("timeout"));
        rc::Transfer transfer;
        transfer.name = QStringLiteral("报告.pdf");
        transfer.entries = 3;
        transfer.doneBytes = 4096;
        transfer.sending = true;
        history.transferred(peer(QStringLiteral("222222222")), transfer);

        const auto entries = history.state().entries;
        QCOMPARE(entries.at(1).kind, QStringLiteral("refused"));
        QCOMPARE(entries.at(1).reason, QStringLiteral("timeout"));
        QCOMPARE(entries.at(0).kind, QStringLiteral("file"));
        QCOMPARE(entries.at(0).file, QStringLiteral("报告.pdf"));
        QCOMPARE(entries.at(0).count, 3);
        QCOMPARE(entries.at(0).bytes, 4096);
        QVERIFY(entries.at(0).sent);
    }

    void recordsUpdates()
    {
        QTemporaryDir directory;
        ConnectionHistory history(directory.filePath(QStringLiteral("history.json")));
        history.updated(QStringLiteral("0.2.1"));
        const auto entries = history.state().entries;
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).kind, QStringLiteral("updated"));
        QCOMPARE(entries.at(0).version, QStringLiteral("0.2.1"));
        QVERIFY(entries.at(0).deviceId.isEmpty());
    }

    void recordsPowerActions()
    {
        QTemporaryDir directory;
        ConnectionHistory history(directory.filePath(QStringLiteral("history.json")));
        history.power({3, QStringLiteral("392955715"), QStringLiteral("书房的台式机"), QStringLiteral("Windows"), true},
                      QStringLiteral("restart"));
        const auto entries = history.state().entries;
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).kind, QStringLiteral("power"));
        QCOMPARE(entries.at(0).deviceId, QStringLiteral("392955715"));
        QCOMPARE(entries.at(0).name, QStringLiteral("书房的台式机"));
        QCOMPARE(entries.at(0).action, QStringLiteral("restart"));
    }

    // 本机帮忙叫醒了我的设备：谁要叫醒它的（网页版是空的），存下来下次还在
    void recordsWakes()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath(QStringLiteral("history.json"));
        {
            ConnectionHistory history(file);
            history.woke(QStringLiteral("392955715"), QStringLiteral("书房的台式机"), QStringLiteral("梨茶的 iPad"));
            history.woke(QStringLiteral("392955715"), QStringLiteral("书房的台式机"), {});
        }
        const auto entries = ConnectionHistory(file).state().entries;
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).kind, QStringLiteral("wake"));
        QCOMPARE(entries.at(0).by, QString());
        QCOMPARE(entries.at(1).deviceId, QStringLiteral("392955715"));
        QCOMPARE(entries.at(1).name, QStringLiteral("书房的台式机"));
        QCOMPARE(entries.at(1).by, QStringLiteral("梨茶的 iPad"));
    }

    // 存在文件里，下次还在；上次还"连着"的当作那时断了；编号接着往下
    void survivesARestart()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath(QStringLiteral("history.json"));
        {
            ConnectionHistory history(file, [] { return qint64(1000); });
            history.connected(peer(QStringLiteral("111111111")), QStringLiteral("s1"));
        }
        ConnectionHistory again(file);
        QCOMPARE(again.state().entries.size(), 1);
        QVERIFY(again.state().entries.at(0).endedAt >= 1000);
        again.wrongPin();
        QCOMPARE(again.state().entries.at(0).id, again.state().entries.at(1).id + 1);
    }

    void keepsOnlyTheNewest()
    {
        QTemporaryDir directory;
        ConnectionHistory history(directory.filePath(QStringLiteral("history.json")));
        for (int i = 0; i < ConnectionHistory::MaxEntries + 5; ++i)
            history.wrongPin();
        QCOMPARE(history.state().entries.size(), ConnectionHistory::MaxEntries);
        QCOMPARE(history.state().entries.first().id, ConnectionHistory::MaxEntries + 5);
    }

    void clears()
    {
        QTemporaryDir directory;
        const QString file = directory.filePath(QStringLiteral("history.json"));
        ConnectionHistory history(file);
        history.wrongPin();
        history.clear();
        QVERIFY(history.state().entries.isEmpty());
        QVERIFY(ConnectionHistory(file).state().entries.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestConnectionHistory)
#include "tst_connection_history.moc"
