#include <rc/chat_history.h>

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

namespace {

rc::ChatMessage said(const QString &text, qint64 time, bool incoming = false)
{
    return {incoming, text, time, {}};
}

} // namespace

class TestChatHistory : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.filePath(QStringLiteral("chats")));
        const QList<rc::ChatMessage> messages{
            said(QStringLiteral("在吗"), 1000, true),
            said(QStringLiteral("在，怎么了"), 2000),
            {false, QStringLiteral("看下这个"), 3000, QStringLiteral("对方已经断开了")},
        };
        QVERIFY(history.save(QStringLiteral("482915307"), messages));
        QCOMPARE(history.load(QStringLiteral("482915307")), messages);
    }

    void neverTalkedIsEmpty()
    {
        QTemporaryDir directory;
        QVERIFY(rc::ChatHistory(directory.path()).load(QStringLiteral("482915307")).isEmpty());
    }

    void keepsTheLatest()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        QList<rc::ChatMessage> messages;
        for (int i = 0; i < rc::ChatHistory::Limit + 20; ++i)
            messages.append(said(QString::number(i), i));
        QVERIFY(history.save(QStringLiteral("482915307"), messages));
        const QList<rc::ChatMessage> loaded = history.load(QStringLiteral("482915307"));
        QCOMPARE(loaded.size(), rc::ChatHistory::Limit);
        QCOMPARE(loaded.first().text, QStringLiteral("20"));
        QCOMPARE(loaded.last().text, QString::number(rc::ChatHistory::Limit + 19));
    }

    // 每台设备一份；按地址记的文件名里没有冒号（Windows 不允许）
    void eachDeviceHasItsOwn()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        QVERIFY(history.save(QStringLiteral("482915307"), {said(QStringLiteral("a"), 1)}));
        QVERIFY(history.save(QStringLiteral("ip:192.168.1.20"), {said(QStringLiteral("b"), 2)}));
        QVERIFY(history.save(QStringLiteral("ip:fe80::1"), {said(QStringLiteral("c"), 3)}));
        QCOMPARE(history.load(QStringLiteral("482915307")).first().text, QStringLiteral("a"));
        QCOMPARE(history.load(QStringLiteral("ip:192.168.1.20")).first().text, QStringLiteral("b"));
        QCOMPARE(history.load(QStringLiteral("ip:fe80::1")).first().text, QStringLiteral("c"));
        for (const QString &name : QDir(directory.path()).entryList(QDir::Files))
            QVERIFY2(!name.contains(QLatin1Char(':')), qUtf8Printable(name));
    }
};

QTEST_GUILESS_MAIN(TestChatHistory)
#include "tst_chat_history.moc"
