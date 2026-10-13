#include "rc/download_mark.h"

#include <QDir>

#include <windows.h>

namespace rc {

void markDownloaded(const QString &path)
{
    // 文件的备用数据流 Zone.Identifier：ZoneId=3 是互联网区域
    const std::wstring stream = QDir::toNativeSeparators(path).toStdWString() + L":Zone.Identifier";
    const HANDLE file = CreateFileW(stream.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        qWarning("没能把 %s 标成来自网络（%lu）", qUtf8Printable(path), GetLastError());
        return;
    }
    static constexpr char Zone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
    DWORD written = 0;
    if (!WriteFile(file, Zone, DWORD(sizeof Zone - 1), &written, nullptr))
        qWarning("没能把 %s 标成来自网络（%lu）", qUtf8Printable(path), GetLastError());
    CloseHandle(file);
}

} // namespace rc
