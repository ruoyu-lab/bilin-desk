#include "chat/conversation.h"

#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestConversation : public QObject
{
    Q_OBJECT

private slots:
    // 每一句都存下来，下次打开还在；按 IP 连的（"ip:地址"）也存得下
    void keepsWhatWasSaid()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        for (const QString &peer : {QStringLiteral("482915307"), QStringLiteral("ip:fe80::1")}) {
            {
                Conversation conversation(history, peer, nullptr);
                conversation.addIncoming(QStringLiteral("在吗"));
                const int id = conversation.addOutgoing(QStringLiteral("在"));
                QVERIFY(conversation.markFailed(id, QStringLiteral("连接断了")));
            }
            Conversation again(history, peer, nullptr);
            const QVariantList messages = again.messages();
            QCOMPARE(messages.size(), 2);
            QCOMPARE(messages.at(0).toMap().value(QStringLiteral("incoming")).toBool(), true);
            QCOMPARE(messages.at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("在吗"));
            QCOMPARE(messages.at(1).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("在"));
            QCOMPARE(messages.at(1).toMap().value(QStringLiteral("error")).toString(), QStringLiteral("连接断了"));
            QCOMPARE(again.unread(), 0);
        }
    }

    // 只有对方说的算没看；看过了清零，只通知一次
    void unreadIsWhatThePeerSaid()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        Conversation conversation(history, QStringLiteral("482915307"), nullptr);
        QSignalSpy unread(&conversation, &Conversation::unreadChanged);
        conversation.addOutgoing(QStringLiteral("你好"));
        conversation.addIncoming(QStringLiteral("你好"));
        conversation.addIncoming(QStringLiteral("帮我看看"));
        QCOMPARE(conversation.unread(), 2);
        QCOMPARE(unread.count(), 2);
        conversation.markRead();
        conversation.markRead();
        QCOMPARE(conversation.unread(), 0);
        QCOMPARE(unread.count(), 3);
    }

    // 编号所有对话共用：别的对话里的那一句不算这份的
    void idsAreSharedAcrossConversations()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        Conversation first(history, QStringLiteral("482915307"), nullptr);
        Conversation second(history, QStringLiteral("193847562"), nullptr);
        const int a = first.addOutgoing(QStringLiteral("a"));
        const int b = second.addOutgoing(QStringLiteral("b"));
        QVERIFY(a != b);
        QVERIFY(!first.markFailed(b, QStringLiteral("没发出去")));
        QVERIFY(second.markFailed(b, QStringLiteral("没发出去")));
    }

    void keepsTheLatest()
    {
        QTemporaryDir directory;
        const rc::ChatHistory history(directory.path());
        Conversation conversation(history, QStringLiteral("482915307"), nullptr);
        for (qsizetype i = 0; i <= rc::ChatHistory::Limit; ++i)
            conversation.addIncoming(QString::number(i));
        const QVariantList messages = conversation.messages();
        QCOMPARE(messages.size(), rc::ChatHistory::Limit);
        QCOMPARE(messages.first().toMap().value(QStringLiteral("text")).toString(), QStringLiteral("1"));
        QCOMPARE(Conversation(history, QStringLiteral("482915307"), nullptr).messages(), messages);
    }

    // 存不下来（目录的位置是个文件）：说明为什么，存下来以后说明没了
    void saysWhenItCannotStore()
    {
        QTemporaryDir directory;
        const QString blocked = directory.filePath(QStringLiteral("chats"));
        QFile file(blocked);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const rc::ChatHistory history(blocked);
        Conversation conversation(history, QStringLiteral("482915307"), nullptr);
        QSignalSpy storeError(&conversation, &Conversation::storeErrorChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("建不了对话记录的目录")));
        conversation.addIncoming(QStringLiteral("在吗"));
        QVERIFY(!conversation.storeError().isEmpty());
        QCOMPARE(storeError.count(), 1);
        QVERIFY(file.remove());
        conversation.addIncoming(QStringLiteral("在吗"));
        QVERIFY(conversation.storeError().isEmpty());
        QCOMPARE(storeError.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestConversation)
#include "tst_conversation.moc"
