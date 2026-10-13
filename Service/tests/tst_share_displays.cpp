#include "share_displays.h"

#include <QTest>

namespace control = rc::control;

namespace {

const control::Display Builtin{1, QStringLiteral("内建视网膜显示器"), 3024, 1964, true};
const control::Display External{2, QStringLiteral("DELL U2720Q"), 3840, 2160, false};

// 记下 ShareDisplays 交回的：换了哪几次显示器，发给了谁、发了什么
struct Host
{
    QList<quint32> shown;
    QList<std::pair<int, control::DisplayList>> sent;
    bool showWorks = true;
    QList<int> viewOnly; // 只能看的观看端

    ShareDisplays displays{
        [this](quint32 id) {
            if (showWorks)
                shown.append(id);
            return showWorks;
        },
        [this](int viewer, const QByteArray &message) { sent.append({viewer, *control::parseDisplays(message)}); },
        [this](int viewer) { return !viewOnly.contains(viewer); },
    };
};

} // namespace

class TestShareDisplays : public QObject
{
    Q_OBJECT

private slots:
    // 连进来的人知道有哪几块，正在分享的是主显示器
    void joinedViewerLearnsTheDisplays()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        host.displays.viewerJoined(3);
        QCOMPARE(host.sent.size(), 1);
        QCOMPARE(host.sent.at(0).first, 3);
        QCOMPARE(host.sent.at(0).second, (control::DisplayList{{Builtin, External}, Builtin.id}));
        QVERIFY(host.shown.isEmpty());
    }

    // 只能看的人换不了：不打断正在控制的人
    void viewOnlyViewerCannotSwitch()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        host.viewOnly = {4};
        host.displays.received(4, control::showDisplay(External.id));
        QVERIFY(host.shown.isEmpty());
        QVERIFY(host.sent.isEmpty());
    }

    // 能控制的人换了大家都跟着：换完告诉所有人
    void controllerSwitchesForEveryone()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        host.displays.received(3, control::showDisplay(External.id));
        QCOMPARE(host.shown, QList<quint32>{External.id});
        QCOMPARE(host.sent.size(), 1);
        QCOMPARE(host.sent.at(0).first, 0);
        QCOMPARE(host.sent.at(0).second.current, External.id);
    }

    void showingTheSameDisplayDoesNothing()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        host.displays.received(3, control::showDisplay(Builtin.id));
        QVERIFY(host.shown.isEmpty());
        QVERIFY(host.sent.isEmpty());
    }

    // 刚拔掉的、换不过去的：还是原来那块
    void unknownOrFailedSwitchKeepsTheDisplay()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        host.displays.received(3, control::showDisplay(9));
        QVERIFY(host.shown.isEmpty());
        host.showWorks = false;
        host.displays.received(3, control::showDisplay(External.id));
        QVERIFY(host.sent.isEmpty());
        QCOMPARE(host.displays.list().current, Builtin.id);
    }

    // 正在分享的那块拔掉了：换回主显示器，告诉所有人
    void unpluggedDisplayFallsBackToMain()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.displays.received(3, control::showDisplay(External.id));
        host.shown.clear();
        host.sent.clear();
        host.displays.setDisplays({Builtin});
        QCOMPARE(host.shown, QList<quint32>{0});
        QCOMPARE(host.sent.size(), 1);
        QCOMPARE(host.sent.at(0), (std::pair{0, control::DisplayList{{Builtin}, Builtin.id}}));
    }

    // 显示器没变（观看端连进来时也会重新读一次）就不再发
    void unchangedDisplaysAreNotSentAgain()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        QCOMPARE(host.sent.size(), 1);
        host.displays.setDisplays({Builtin, External});
        QCOMPARE(host.sent.size(), 1);
    }

    // 没人在看了：下一个人从主显示器看起
    void nextViewerStartsOnTheMainDisplay()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.displays.received(3, control::showDisplay(External.id));
        host.displays.showMain();
        QCOMPARE(host.shown, (QList<quint32>{External.id, 0}));
        QCOMPARE(host.displays.list().current, Builtin.id);
        host.displays.showMain();
        QCOMPARE(host.shown.size(), 2);
    }

    void ignoresOtherMessages()
    {
        Host host;
        host.displays.setDisplays({Builtin, External});
        host.sent.clear();
        QTest::ignoreMessage(QtWarningMsg, "观看端 3 发来不认识的控制消息");
        host.displays.received(3, R"({"type":"something"})");
        QVERIFY(host.shown.isEmpty());
        QVERIFY(host.sent.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestShareDisplays)
#include "tst_share_displays.moc"
