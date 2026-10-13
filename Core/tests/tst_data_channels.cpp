#include <rc/data_channels.h>
#include <rc/peer_protocol.h>

#include <QRegularExpression>
#include <QTest>

class TestDataChannels : public QObject
{
    Q_OBJECT

private slots:
    void eachChannelGoesToItsHandler()
    {
        rc::peer::DataChannels channels;
        QList<std::pair<int, QByteArray>> chat;
        QList<QByteArray> other;
        channels.add(rc::peer::ChatChannel,
                     [&chat](int viewer, const QByteArray &data) { chat.append({viewer, data}); });
        channels.add(7, [&other](int, const QByteArray &data) { other.append(data); });

        channels.receive(rc::peer::ChatChannel, 3, "hello");
        channels.receive(7, 0, "x");
        QCOMPARE(chat, (QList<std::pair<int, QByteArray>>{{3, "hello"}}));
        QCOMPARE(other, QList<QByteArray>{"x"});
    }

    void unknownChannelIsDropped()
    {
        rc::peer::DataChannels channels;
        bool called = false;
        channels.add(rc::peer::ChatChannel, [&called](int, const QByteArray &) { called = true; });
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("没有人处理")));
        channels.receive(9, 1, "?");
        QVERIFY(!called);
    }
};

QTEST_GUILESS_MAIN(TestDataChannels)
#include "tst_data_channels.moc"
