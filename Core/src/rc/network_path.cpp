#include "rc/network_path.h"

#include <QHostAddress>

namespace rc {

NetworkPath networkPath(QStringView path)
{
    if (path.startsWith(u"relay "))
        return NetworkPath::Relay;
    if (!path.startsWith(u"P2P "))
        return NetworkPath::Unknown;
    if (path.endsWith(u"(relay)"))
        return NetworkPath::Relay;
    // 对方地址在 " <-> " 和 " (" 之间："地址:端口"，IPv6 是 "[地址]:端口"
    const qsizetype from = path.indexOf(u" <-> ") + 5;
    const qsizetype port = path.lastIndexOf(u':');
    if (from < 5 || port < from)
        return NetworkPath::Unknown;
    QStringView remote = path.sliced(from, port - from);
    if (remote.startsWith(u'[') && remote.endsWith(u']'))
        remote = remote.sliced(1, remote.size() - 2);
    const QHostAddress address(remote.toString());
    if (address.isPrivateUse() || address.isLinkLocal() || address.isLoopback())
        return NetworkPath::Lan;
    return NetworkPath::Direct;
}

} // namespace rc
