#include "session/viewer_audio.h"

#include <rc/control_protocol.h>

#include <QTest>

class TestViewerAudio : public QObject
{
    Q_OBJECT

private slots:
    // 对方录不了声音：说为什么；好了说明就没了
    void peerProblemIsTold()
    {
        ViewerAudio audio;
        QVERIFY(audio.note().isEmpty());
        QVERIFY(audio.setPeerProblem(rc::control::SoundNoDevice));
        QCOMPARE(audio.note(), QStringLiteral("对方电脑没有能用的声音设备，传不了声音"));
        QVERIFY(!audio.setPeerProblem(rc::control::SoundNoDevice));
        QVERIFY(audio.setPeerProblem(rc::control::SoundNoPermission));
        QCOMPARE(audio.note(), QStringLiteral("对方电脑没有录声音的授权"));
        QVERIFY(audio.setPeerProblem(rc::control::SoundFailed));
        QCOMPARE(audio.note(), QStringLiteral("对方电脑录不了声音"));
        QVERIFY(audio.setPeerProblem({}));
        QVERIFY(audio.note().isEmpty());
    }

    // 本机关着远程声音时不说对方的
    void quietWhenTurnedOff()
    {
        ViewerAudio audio;
        audio.setPeerProblem(rc::control::SoundFailed);
        QVERIFY(audio.setEnabled(false, nullptr));
        QVERIFY(audio.note().isEmpty());
        QVERIFY(!audio.setPeerProblem(rc::control::SoundNoDevice));
        QVERIFY(audio.setEnabled(true, nullptr));
        QCOMPARE(audio.note(), QStringLiteral("对方电脑没有能用的声音设备，传不了声音"));
    }
};

QTEST_GUILESS_MAIN(TestViewerAudio)
#include "tst_viewer_audio.moc"
