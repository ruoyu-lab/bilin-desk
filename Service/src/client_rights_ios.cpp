#include "client_rights.h"

// iOS：后台服务在程序自己的进程里，本机连接的那个文件在程序的沙盒目录里，别的程序碰不到：连上来的就是本程序
//（每个程序都是同一个系统用户 mobile，按用户分不出来）。读写文件用的也是它自己的权限
namespace client_rights {

bool admit(qintptr)
{
    return true;
}

bool mayRead(qintptr, const QStringList &)
{
    return true;
}

bool mayWrite(qintptr, const QString &)
{
    return true;
}

} // namespace client_rights
