#include "app_keeper.h"

#include <QTest>

// 记下 AppKeeper 做了什么：拉了几次界面程序、断开了几次
struct Keeper
{
    int launches = 0;
    int disconnects = 0;
    AppKeeper::Launch result = AppKeeper::Launch::Started;

    AppKeeper keeper{
        [this] {
            ++launches;
            return result;
        },
        [this] { ++disconnects; },
        50,
    };
};

class TestAppKeeper : public QObject
{
    Q_OBJECT

private slots:
    // 界面在、或者没人连着：什么都不做
    void nothingToDoWhileAttendedOrUnwatched()
    {
        Keeper k;
        k.keeper.setAttended(true);
        k.keeper.setWatched(true);
        k.keeper.setWatched(false);
        k.keeper.setAttended(false);
        QTest::qWait(100);
        QCOMPARE(k.launches, 0);
        QCOMPARE(k.disconnects, 0);
    }

    // 有人连着，界面意外没了：拉起来；连上了就不断开
    void appGoneIsLaunchedAgain()
    {
        Keeper k;
        k.keeper.setAttended(true);
        k.keeper.setWatched(true);
        k.keeper.setAttended(false);
        QCOMPARE(k.launches, 1);
        k.keeper.setAttended(true);
        QTest::qWait(100);
        QCOMPARE(k.disconnects, 0);
    }

    // 拉起来以后一直没连上：断开连着的人
    void appThatNeverComesBackDisconnects()
    {
        Keeper k;
        k.keeper.setWatched(true);
        QCOMPARE(k.launches, 1);
        QTRY_COMPARE(k.disconnects, 1);
    }

    // 拉不起来：马上断开
    void appThatCannotStartDisconnects()
    {
        Keeper k;
        k.result = AppKeeper::Launch::Failed;
        k.keeper.setWatched(true);
        QCOMPARE(k.launches, 1);
        QCOMPARE(k.disconnects, 1);
    }

    // 电脑前没人登录（装好时的登录界面）：连着的不断开
    void nobodyThereKeepsTheConnection()
    {
        Keeper k;
        k.result = AppKeeper::Launch::NobodyThere;
        k.keeper.setWatched(true);
        QCOMPARE(k.launches, 1);
        QTest::qWait(100);
        QCOMPARE(k.disconnects, 0);
    }

    // 界面说它要退出：断开连着的人，界面没了也不拉
    void quittingAppIsNotLaunched()
    {
        Keeper k;
        k.keeper.setAttended(true);
        k.keeper.setWatched(true);
        k.keeper.appQuitting();
        QCOMPARE(k.disconnects, 1);
        k.keeper.setAttended(false);
        QTest::qWait(100);
        QCOMPARE(k.launches, 0);
        QCOMPARE(k.disconnects, 1);
        // 下次界面连上以后又照常看着
        k.keeper.setAttended(true);
        k.keeper.setAttended(false);
        QCOMPARE(k.launches, 1);
    }
};

QTEST_GUILESS_MAIN(TestAppKeeper)
#include "tst_app_keeper.moc"
