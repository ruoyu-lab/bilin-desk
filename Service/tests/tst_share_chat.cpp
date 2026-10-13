#include "share_chat.h"

#include <rc/chat_protocol.h>

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

namespace protocol = rc::protocol;
using rc::peer::SendResult;

namespace {

protocol::SharePeer viewer(int id)
{
    return {id, QStringLiteral("22222222%1").arg(id), QStringLiteral("小林的笔记本"), QStringLiteral("Windows"), false};
}

} // namespace

// 对方那一头是假的：记下发给谁、发了什么，按 result 回答
class TestShareChat : public QObject
{
    Q_OBJECT

private:
    struct Sent
    {
        int id;
        rc::chat::Message message;
    };
    QList<Sent> sent_;
    SendResult result_ = SendResult::Sent;

    ShareChat::Sender sender()
    {
        return [this](int id, const QByteArray &data) {
            sent_.append({id, *rc::chat::parse(data)});
            return result_;
        };
    }

private slots:
    void init()
    {
        sent_.clear();
        result_ = SendResult::Sent;
    }

    void newViewersHearWhetherAnyoneIsThere()
    {
        ShareChat chat(sender());
        chat.setAttended(true);
        QVERIFY(sent_.isEmpty());
        chat.setViewers({viewer(1)});
        QCOMPARE(sent_.size(), 1);
        QCOMPARE(sent_.at(0).id, 1);
        QCOMPARE(sent_.at(0).message, rc::chat::Message(rc::chat::Presence{true}));
        // 已经告诉过的不再说
        chat.setViewers({viewer(1), viewer(2)});
        QCOMPARE(sent_.size(), 2);
        QCOMPARE(sent_.at(1).id, 2);
    }

    void everyoneHearsWhenTheAppGoes()
    {
        ShareChat chat(sender());
        chat.setAttended(true);
        chat.setViewers({viewer(1), viewer(2)});
        sent_.clear();
        chat.setAttended(false);
        QCOMPARE(sent_.size(), 2);
        QCOMPARE(sent_.at(0).message, rc::chat::Message(rc::chat::Presence{false}));
        QCOMPARE(sent_.at(1).id, 2);
        // 走了的不再告诉
        chat.setViewers({viewer(2)});
        sent_.clear();
        chat.setAttended(true);
        QCOMPARE(sent_.size(), 1);
        QCOMPARE(sent_.at(0).id, 2);
    }

    void textReachesTheApp()
    {
        ShareChat chat(sender());
        QSignalSpy received(&chat, &ShareChat::received);
        chat.setAttended(true);
        chat.receive(1, rc::chat::text(QStringLiteral("帮我看下这个报错")));
        QCOMPARE(received.count(), 1);
        QCOMPARE(received.at(0), (QVariantList{1, QStringLiteral("帮我看下这个报错")}));
    }

    void nobodyThereDropsText()
    {
        ShareChat chat(sender());
        QSignalSpy received(&chat, &ShareChat::received);
        chat.receive(1, rc::chat::text(QStringLiteral("在吗")));
        QCOMPARE(received.count(), 0);
    }

    void brokenMessagesAreDropped()
    {
        ShareChat chat(sender());
        QSignalSpy received(&chat, &ShareChat::received);
        chat.setAttended(true);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("不对")));
        chat.receive(1, "{");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("只有被控的一方")));
        chat.receive(1, rc::chat::presence(true));
        QCOMPARE(received.count(), 0);
    }

    void sendsWhatTheAppSays()
    {
        ShareChat chat(sender());
        QSignalSpy failed(&chat, &ShareChat::failed);
        chat.send(1, 7, QStringLiteral("好的"));
        QCOMPARE(sent_.size(), 1);
        QCOMPARE(sent_.at(0).id, 1);
        QCOMPARE(sent_.at(0).message, rc::chat::Message(rc::chat::Text{QStringLiteral("好的")}));
        QCOMPARE(failed.count(), 0);
    }

    void failuresGoBackToTheApp()
    {
        ShareChat chat(sender());
        QSignalSpy failed(&chat, &ShareChat::failed);
        result_ = SendResult::QueueFull;
        chat.send(1, 7, QStringLiteral("好的"));
        result_ = SendResult::NotConnected;
        chat.send(2, 8, QStringLiteral("还在吗"));
        QCOMPARE(failed.count(), 2);
        QCOMPARE(failed.at(0), (QVariantList{1, 7, QString(protocol::QueueFull)}));
        QCOMPARE(failed.at(1), (QVariantList{2, 8, QString(protocol::NotConnected)}));
    }
};

QTEST_GUILESS_MAIN(TestShareChat)
#include "tst_share_chat.moc"
