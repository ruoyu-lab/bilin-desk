#include "rc/machine.h"

#include <QCryptographicHash>

namespace rc {

QString machineHash()
{
    const QString id = machineId();
    if (id.isEmpty())
        return {};
    return QString::fromLatin1(
        QCryptographicHash::hash("bilin-machine:" + id.toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace rc
