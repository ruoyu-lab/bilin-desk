#include "rc/release.h"

#include <QSysInfo>

namespace rc::release {

QString platform()
{
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64");
#elif defined(Q_OS_WIN) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("windows-x64");
#elif defined(Q_OS_ANDROID) && defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("android-arm64");
#else
    return {};
#endif
}

QByteArray text(const QString &platform, const QString &version, qint64 size, const QByteArray &sha256Hex)
{
    return "bilin-release\nplatform=" + platform.toUtf8() + "\nversion=" + version.toUtf8()
           + "\nsize=" + QByteArray::number(size) + "\nsha256=" + sha256Hex + '\n';
}

QByteArray publicKey()
{
    return QByteArray::fromBase64(RC_RELEASE_KEY);
}

} // namespace rc::release
