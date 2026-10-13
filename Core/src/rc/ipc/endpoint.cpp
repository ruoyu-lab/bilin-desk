#include "rc/ipc/endpoint.h"

#include "rc/paths.h"

#include <QCryptographicHash>
#include <QDir>

namespace rc::ipc {

QString userScopedName(const QString &base)
{
    // 按主目录区分用户：Windows 的命名管道是全机共用的名字。换了数据目录（RC_STATE_DIR）的，按那个目录区分
    const QByteArray user = qEnvironmentVariable(StateDirectoryVariable, QDir::homePath()).toUtf8();
    return base + u'-' + QString::fromLatin1(QCryptographicHash::hash(user, QCryptographicHash::Sha1).toHex().left(12));
}

QString serviceEndpoint()
{
    return userScopedName(QStringLiteral("com.bilindesk.bilin.service"));
}

} // namespace rc::ipc
