#include "client_rights.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <windows.h>

#include <memory>
#include <vector>

namespace client_rights {

namespace {

struct HandleCloser
{
    void operator()(HANDLE handle) const { CloseHandle(handle); }
};
using Handle = std::unique_ptr<void, HandleCloser>;

DWORD clientProcess(qintptr socket)
{
    ULONG process = 0;
    return GetNamedPipeClientProcessId(reinterpret_cast<HANDLE>(socket), &process) ? process : 0;
}

// 客户端的身份，可以拿来核对它能不能访问（AccessCheck 要模拟级别的令牌）
Handle clientToken(qintptr socket)
{
    const Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, clientProcess(socket)));
    HANDLE primary = nullptr;
    if (!process || !OpenProcessToken(process.get(), TOKEN_QUERY | TOKEN_DUPLICATE, &primary))
        return {};
    const Handle owned(primary);
    HANDLE impersonation = nullptr;
    if (!DuplicateToken(primary, SecurityImpersonation, &impersonation))
        return {};
    return Handle(impersonation);
}

bool isAdministrator(HANDLE token)
{
    BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD size = sizeof sid;
    BOOL member = FALSE;
    return CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &size)
           && CheckTokenMembership(token, sid, &member) && member;
}

bool allowed(HANDLE token, const QString &path, DWORD desired)
{
    const std::wstring name = QDir::toNativeSeparators(path).toStdWString();
    constexpr SECURITY_INFORMATION what = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    DWORD needed = 0;
    GetFileSecurityW(name.c_str(), what, nullptr, 0, &needed);
    if (needed == 0)
        return false;
    std::vector<BYTE> descriptor(needed);
    if (!GetFileSecurityW(name.c_str(), what, descriptor.data(), needed, &needed))
        return false;
    GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
    MapGenericMask(&desired, &mapping);
    PRIVILEGE_SET privileges{};
    DWORD privilegesSize = sizeof privileges;
    DWORD granted = 0;
    BOOL status = FALSE;
    return AccessCheck(descriptor.data(), token, desired, &mapping, &privileges, &privilegesSize, &granted, &status)
           && status;
}

// 实际指向的那一个（跟着符号链接、目录联接走）
QString finalPath(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? path : canonical;
}

} // namespace

bool admit(qintptr socket)
{
    const DWORD process = clientProcess(socket);
    DWORD session = 0;
    if (process == 0 || !ProcessIdToSessionId(process, &session))
        return false;
    if (session == WTSGetActiveConsoleSessionId())
        return true;
    const Handle token = clientToken(socket);
    return token && isAdministrator(token.get());
}

bool mayRead(qintptr socket, const QStringList &paths)
{
    const Handle token = clientToken(socket);
    if (!token)
        return false;
    for (const QString &path : paths) {
        const QString target = finalPath(path);
        const QFileInfo info(target);
        if (!allowed(token.get(), target, info.isDir() ? FILE_LIST_DIRECTORY | FILE_TRAVERSE : FILE_GENERIC_READ))
            return false;
        if (!info.isDir())
            continue;
        // 文件夹里的每一项：SDK 列出来、读出来的都要是这个用户自己读得到的（不跟着链接走，SDK 也不跟）
        QDirIterator entries(target, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                             QDirIterator::Subdirectories);
        while (entries.hasNext()) {
            const QFileInfo entry = entries.nextFileInfo();
            if (!allowed(token.get(), entry.filePath(), entry.isDir() ? FILE_LIST_DIRECTORY | FILE_TRAVERSE : FILE_GENERIC_READ))
                return false;
        }
    }
    return true;
}

bool mayWrite(qintptr socket, const QString &directory)
{
    const Handle token = clientToken(socket);
    return token && allowed(token.get(), finalPath(directory), FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY);
}

} // namespace client_rights
