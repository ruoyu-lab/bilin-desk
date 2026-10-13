#include <rc/chat_protocol.h>

#include <QTest>

class TestChatProtocol : public QObject
{
    Q_OBJECT

private slots:
    void textRoundTrips()
    {
        const QString words = QStringLiteral("晚上一起吃饭吗？\n八点见 😀");
        QCOMPARE(rc::chat::parse(rc::chat::text(words)), rc::chat::Message(rc::chat::Text{words}));
    }

    void presenceRoundTrips()
    {
        QCOMPARE(rc::chat::parse(rc::chat::presence(false)), rc::chat::Message(rc::chat::Presence{false}));
        QCOMPARE(rc::chat::parse(rc::chat::presence(true)), rc::chat::Message(rc::chat::Presence{true}));
    }

    void longestTextIsAccepted()
    {
        const QString words(rc::chat::MaxText, QLatin1Char('a'));
        QCOMPARE(rc::chat::parse(rc::chat::text(words)), rc::chat::Message(rc::chat::Text{words}));
    }

    void rejectsOthers_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("not json") << QByteArray("hello");
        QTest::newRow("no type") << QByteArray(R"({"text":"hi"})");
        QTest::newRow("unknown type") << QByteArray(R"({"type":"typing"})");
        QTest::newRow("empty text") << QByteArray(R"({"type":"text","text":"  "})");
        QTest::newRow("too long") << rc::chat::text(QString(rc::chat::MaxText + 1, QLatin1Char('a')));
    }

    void rejectsOthers()
    {
        QFETCH(QByteArray, data);
        QVERIFY(!rc::chat::parse(data));
    }
};

QTEST_GUILESS_MAIN(TestChatProtocol)
#include "tst_chat_protocol.moc"
