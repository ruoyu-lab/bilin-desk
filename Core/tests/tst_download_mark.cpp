#include <rc/download_mark.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#else
#include <QDir>

#include <windows.h>
#endif

namespace {

// 系统认不认它是从网上来的
bool markedAsDownloaded(const QString &path)
{
#ifdef Q_OS_MACOS
    return getxattr(QFile::encodeName(path).constData(), "com.apple.quarantine", nullptr, 0, 0, 0) > 0;
#else
    const std::wstring stream = QDir::toNativeSeparators(path).toStdWString() + L":Zone.Identifier";
    const HANDLE file = CreateFileW(stream.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    char zone[64] = {};
    DWORD read = 0;
    const bool ok = ReadFile(file, zone, sizeof zone - 1, &read, nullptr);
    CloseHandle(file);
    return ok && QByteArray(zone, int(read)).contains("ZoneId=3");
#endif
}

} // namespace

class TestDownloadMark : public QObject
{
    Q_OBJECT

private slots:
    void receivedFileIsMarked()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("收到的 报告.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        QVERIFY(!markedAsDownloaded(path));
        rc::markDownloaded(path);
        QVERIFY(markedAsDownloaded(path));
        // 内容不变
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("hello"));
    }
};

QTEST_GUILESS_MAIN(TestDownloadMark)
#include "tst_download_mark.moc"
