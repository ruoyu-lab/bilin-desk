#include "autostart.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>

// 假的系统登录项：测试不碰真的（macOS 的登录项、Windows 的注册表）
namespace fake {

login_item::Status status = login_item::Status::Off;
// 登记以后系统还要用户批准
bool needsApproval = false;
// 系统不让改：改了也不变
bool refuses = false;
int changes = 0;
int settingsOpened = 0;
std::function<void(bool)> launched;

} // namespace fake

namespace login_item {

Status status()
{
    return fake::status;
}

void setEnabled(bool enabled)
{
    ++fake::changes;
    if (fake::refuses)
        return;
    fake::status = !enabled ? Status::Off : fake::needsApproval ? Status::NeedsApproval : Status::On;
}

void openSystemSettings()
{
    ++fake::settingsOpened;
}

void whenLaunched(std::function<void(bool atLogin)> handler)
{
    fake::launched = std::move(handler);
}

} // namespace login_item

class TestAutostart : public QObject
{
    Q_OBJECT

private slots:
    // 偏好设置写进临时目录，不碰真的
    void initTestCase()
    {
        QVERIFY(settingsDir_.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("BilinTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_autostart"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir_.path());
    }

    void init()
    {
        QSettings().clear();
        fake::status = login_item::Status::Off;
        fake::needsApproval = false;
        fake::refuses = false;
        fake::changes = 0;
        fake::settingsOpened = 0;
        fake::launched = nullptr;
    }

    void firstRunTurnsOn()
    {
        Autostart autostart(nullptr);
        QVERIFY(autostart.enabled());
        QVERIFY(autostart.problem().isEmpty());
        QCOMPARE(fake::changes, 1);
    }

    // 用户在系统设置里关掉了：之后启动不再替他打开
    void laterRunsLeaveSystemAlone()
    {
        { Autostart first(nullptr); }
        fake::status = login_item::Status::Off;
        fake::changes = 0;
        Autostart autostart(nullptr);
        QVERIFY(!autostart.enabled());
        QCOMPARE(fake::changes, 0);
    }

    void staysOffAfterUserTurnedItOff()
    {
        {
            Autostart first(nullptr);
            QSignalSpy changed(&first, &Autostart::changed);
            first.setEnabled(false);
            QVERIFY(!first.enabled());
            QCOMPARE(changed.count(), 1);
        }
        Autostart autostart(nullptr);
        QVERIFY(!autostart.enabled());
        QCOMPARE(fake::changes, 2);
    }

    // 装好了系统服务：开关也是后台服务那边的"没人开界面时也接受我的设备连接"，两个都开着才算开
    void followsTheService()
    {
        // 登录项早就开着，后台服务那边关着
        fake::status = login_item::Status::On;
        Autostart autostart(nullptr);
        bool unattended = false;
        autostart.followService([&unattended] { return unattended; }, [&unattended](bool value) { unattended = value; });
        QVERIFY(!autostart.enabled());

        // 打开：只改后台服务那边的，登录项不动
        fake::changes = 0;
        autostart.setEnabled(true);
        QVERIFY(unattended);
        QVERIFY(autostart.enabled());
        QCOMPARE(fake::changes, 0);

        autostart.setEnabled(false);
        QVERIFY(!unattended);
        QVERIFY(!autostart.enabled());
        QCOMPARE(fake::status, login_item::Status::Off);
    }

    void approvalNeeded()
    {
        fake::needsApproval = true;
        Autostart autostart(nullptr);
        QVERIFY(!autostart.enabled());
        QCOMPARE(autostart.problem(), QStringLiteral("approval"));
        autostart.openSystemSettings();
        QCOMPARE(fake::settingsOpened, 1);
    }

    void refusedChange()
    {
        fake::refuses = true;
        {
            Autostart first(nullptr);
            QVERIFY(!first.enabled());
            QCOMPARE(first.problem(), QStringLiteral("refused"));
            // 再拨一下开关：还是没改成，也要通知，开关回到关着
            QSignalSpy changed(&first, &Autostart::changed);
            first.setEnabled(true);
            QCOMPARE(changed.count(), 1);
            QCOMPARE(first.problem(), QStringLiteral("refused"));
        }
        // 第一次运行时没开成：之后不再自动去开，也不再说没改成
        fake::changes = 0;
        Autostart autostart(nullptr);
        QCOMPARE(fake::changes, 0);
        QVERIFY(autostart.problem().isEmpty());
    }

    void launchedAtLogin()
    {
        Autostart autostart(nullptr);
        QSignalSpy launched(&autostart, &Autostart::launched);
        fake::launched(true);
        QCOMPARE(launched.count(), 1);
        QCOMPARE(launched.takeFirst().at(0).toBool(), true);
    }

private:
    QTemporaryDir settingsDir_;
};

QTEST_MAIN(TestAutostart)
#include "tst_autostart.moc"
