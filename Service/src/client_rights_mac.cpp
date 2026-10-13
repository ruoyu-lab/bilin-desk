#include "client_rights.h"

#include <SystemConfiguration/SystemConfiguration.h>

#include <unistd.h>

namespace client_rights {

namespace {

// 控制台前的用户；登录窗口上（没人登录）是 root
uid_t consoleUser()
{
    uid_t uid = 0;
    if (CFStringRef name = SCDynamicStoreCopyConsoleUser(nullptr, &uid, nullptr))
        CFRelease(name);
    return uid;
}

} // namespace

bool admit(qintptr socket)
{
    uid_t uid = 0;
    gid_t gid = 0;
    if (getpeereid(int(socket), &uid, &gid) != 0)
        return false;
    return uid == 0 || uid == consoleUser();
}

// 读写文件的是控制台前的用户自己的助手（SDK 的文件传输模块的那一半在它那里）：用的就是他自己的权限
bool mayRead(qintptr, const QStringList &)
{
    return true;
}

bool mayWrite(qintptr, const QString &)
{
    return true;
}

} // namespace client_rights
