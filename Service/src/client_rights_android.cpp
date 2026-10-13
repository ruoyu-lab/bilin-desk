#include "client_rights.h"

#include <sys/socket.h>
#include <unistd.h>

// Android：后台服务在程序自己的进程里，每个程序是系统里单独的一个用户。连上来的进程和本程序是同一个用户才认，
// 读写文件用的也是它自己的权限
namespace client_rights {

bool admit(qintptr socket)
{
    ucred peer{};
    socklen_t size = sizeof peer;
    if (getsockopt(int(socket), SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0)
        return false;
    return peer.uid == getuid();
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
