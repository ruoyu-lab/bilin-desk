#include "setup/setup_steps.h"

#include <QTest>

class TestSetupSteps : public QObject
{
    Q_OBJECT

private slots:
    // 都成了：按顺序做完，进度走到头
    void runsEveryStepInOrder()
    {
        QStringList done;
        double last = 0;
        const QString error = runSetupSteps(
            {
                {QStringLiteral("一"), 1, [&](const auto &progress) {
                     progress(0.5);
                     done.append(QStringLiteral("一"));
                     return QString();
                 }, {}},
                {QStringLiteral("二"), 3, [&](const auto &) {
                     done.append(QStringLiteral("二"));
                     return QString();
                 }, {}},
            },
            [&](double total) {
                QVERIFY(total >= last);
                last = total;
            });
        QVERIFY(error.isEmpty());
        QCOMPARE(done, (QStringList{QStringLiteral("一"), QStringLiteral("二")}));
        QCOMPARE(last, 1.0);
    }

    // 一步没成：后面的不做，做过的倒着撤回（没成的那一步自己不撤）
    void undoesWhatWasDoneWhenAStepFails()
    {
        QStringList log;
        const auto step = [&log](const QString &name, bool ok) {
            return SetupStep{name, 1,
                             [&log, name, ok](const auto &) {
                                 log.append(name);
                                 return ok ? QString() : QStringLiteral("坏了");
                             },
                             [&log, name] { log.append(QStringLiteral("撤") + name); }};
        };
        const QString error = runSetupSteps({step(QStringLiteral("一"), true), step(QStringLiteral("二"), true),
                                             step(QStringLiteral("三"), false), step(QStringLiteral("四"), true)},
                                            [](double) {});
        QCOMPARE(error, QStringLiteral("坏了"));
        QCOMPARE(log, (QStringList{QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"), QStringLiteral("撤二"),
                                   QStringLiteral("撤一")}));
    }
};

QTEST_GUILESS_MAIN(TestSetupSteps)
#include "tst_setup_steps.moc"
