#include <rc/feedback_logs.h>

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using rc::feedback::Log;

class TestFeedbackLogs : public QObject
{
    Q_OBJECT

private:
    static QString write(const QTemporaryDir &directory, const QString &name, const QByteArray &content)
    {
        const QString path = directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        return path;
    }

private slots:
    void readsWholeSmallLog()
    {
        QTemporaryDir directory;
        const QString path = write(directory, QStringLiteral("service.log"), "第一行\n第二行\n");
        QCOMPARE(rc::feedback::readLog(path), Log(QStringLiteral("service.log"), QStringLiteral("第一行\n第二行\n")));
    }

    // 太大的只带末尾，从截下来那一段的第一个整行开始
    void keepsTailFromWholeLine()
    {
        QTemporaryDir directory;
        QByteArray content;
        for (int i = 0; content.size() < rc::feedback::MaxLogBytes * 2; ++i)
            content += "2026-10-19 21:03:11.000 I 第 " + QByteArray::number(i) + " 行\n";
        content += "最后一行\n";
        const std::optional<Log> log = rc::feedback::readLog(write(directory, QStringLiteral("app.log"), content));
        QVERIFY(log);
        const QByteArray text = log->text.toUtf8();
        QVERIFY(text.size() <= rc::feedback::MaxLogBytes);
        QVERIFY(text.startsWith("2026-10-19"));
        QVERIFY(text.endsWith("最后一行\n"));
    }

    void missingAndEmptyAreLeftOut()
    {
        QTemporaryDir directory;
        const QString empty = write(directory, QStringLiteral("app.log.previous"), {});
        const QString present = write(directory, QStringLiteral("app.log"), "有\n");
        const QList<Log> logs = rc::feedback::readLogs({directory.filePath(QStringLiteral("agent.log")), empty, present});
        QCOMPARE(logs, QList<Log>{Log(QStringLiteral("app.log"), QStringLiteral("有\n"))});
    }

    void packRoundTrip()
    {
        const QList<Log> logs{Log(QStringLiteral("app.log"), QStringLiteral("连接 482915736：直连\n")),
                              Log(QStringLiteral("service.log"), QString(10000, u'x'))};
        const QJsonArray packed = rc::feedback::packLogs(logs);
        QVERIFY(packed.at(1).toObject().value(QLatin1StringView("data")).toString().size() < 1000);
        QCOMPARE(rc::feedback::unpackLogs(packed), logs);
    }

    // 说解开以后很大的、解不开的、没名字的：不要
    void rejectsBadPackedLogs()
    {
        const QByteArray big = qCompress(QByteArray(rc::feedback::MaxLogBytes + 1, 'x'));
        const auto item = [](const QString &name, const QByteArray &data) {
            return QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("data"), QString::fromLatin1(data.toBase64())}};
        };
        QVERIFY(!rc::feedback::unpackLogs({item(QStringLiteral("app.log"), big)}));
        QVERIFY(!rc::feedback::unpackLogs({item(QStringLiteral("app.log"), "\x00\x00\x00\x10garbage")}));
        QVERIFY(!rc::feedback::unpackLogs({item({}, qCompress("有"))}));
        QCOMPARE(rc::feedback::unpackLogs({}), QList<Log>{});
    }
};

QTEST_GUILESS_MAIN(TestFeedbackLogs)
#include "tst_feedback_logs.moc"
