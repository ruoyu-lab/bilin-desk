#include <rc/control_protocol.h>

#include <QTest>

class TestControlProtocol : public QObject
{
    Q_OBJECT

private slots:
    void displaysRoundTrip()
    {
        // macOS 的显示器编号是 CGDirectDisplayID，可能很大
        const rc::control::DisplayList list{
            {{1, QStringLiteral("内建视网膜显示器"), 3024, 1964, true}, {4128835121u, QString(), 3840, 2160, false}},
            4128835121u,
        };
        QCOMPARE(rc::control::parseDisplays(rc::control::displays(list)), list);
    }

    void showDisplayRoundTrips()
    {
        QCOMPARE(rc::control::parseShowDisplay(rc::control::showDisplay(4128835121u)), 4128835121u);
    }

    void mainViewerRoundTrips()
    {
        QCOMPARE(rc::control::parseMainViewer(rc::control::mainViewer(false)), false);
        QCOMPARE(rc::control::parseMainViewer(rc::control::mainViewer(true)), true);
    }

    void soundRoundTrips()
    {
        QCOMPARE(rc::control::parseSound(rc::control::sound(rc::control::SoundNoDevice)), QString(rc::control::SoundNoDevice));
        QCOMPARE(rc::control::parseSound(rc::control::sound({})), QString());
    }

    void encodersRoundTrip()
    {
        QCOMPARE(rc::control::parseEncoders(rc::control::encoders(0b110)), quint32(0b110));
        QVERIFY(!rc::control::parseEncoders(rc::control::sound({})));
    }

    void lowLatencyRoundTrips()
    {
        QCOMPARE(rc::control::parseLowLatency(rc::control::lowLatency(true)), true);
        QCOMPARE(rc::control::parseLowLatency(rc::control::lowLatency(false)), false);
        QVERIFY(!rc::control::parseLowLatency(rc::control::thumbnails()));
    }

    // 缩略图的像素压缩后带过去；像素的大小和宽高对不上的不要
    void thumbnailRoundTrips()
    {
        QVERIFY(rc::control::isThumbnails(rc::control::thumbnails()));
        QVERIFY(!rc::control::isThumbnails(rc::control::lowLatency(true)));
        const rc::control::DisplayPicture picture{7, 4, 2, QByteArray(4 * 2 * 4, '\x5a')};
        QCOMPARE(rc::control::parseThumbnail(rc::control::thumbnail(picture)), picture);
        const rc::control::DisplayPicture wrong{7, 4, 3, QByteArray(4 * 2 * 4, '\x5a')};
        QVERIFY(!rc::control::parseThumbnail(rc::control::thumbnail(wrong)));
    }

    // 同一个通道上别的消息不是它们
    void tellsTheKindsApart()
    {
        QVERIFY(!rc::control::parseShowDisplay(rc::control::displays({})));
        QVERIFY(!rc::control::parseDisplays(rc::control::showDisplay(1)));
        QVERIFY(!rc::control::parseMainViewer(rc::control::showDisplay(1)));
        QVERIFY(!rc::control::parseDisplays(rc::control::mainViewer(true)));
        QVERIFY(!rc::control::parseSound(rc::control::mainViewer(true)));
        QVERIFY(!rc::control::parseMainViewer(rc::control::sound(rc::control::SoundFailed)));
        QVERIFY(!rc::control::parseDisplays("hello"));
        QVERIFY(!rc::control::parseShowDisplay(R"({"type":"chat","id":1})"));
    }
};

QTEST_GUILESS_MAIN(TestControlProtocol)
#include "tst_control_protocol.moc"
