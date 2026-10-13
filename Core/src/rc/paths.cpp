#include "rc/paths.h"

#include <QDir>
#include <QStandardPaths>

namespace rc {

QString stateDirectory()
{
    const QString directory = qEnvironmentVariableIsEmpty(StateDirectoryVariable)
                                  ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                        + QStringLiteral("/Bilin")
                                  : qEnvironmentVariable(StateDirectoryVariable);
    if (!QDir().mkpath(directory))
        qWarning("建不了状态目录 %s", qUtf8Printable(directory));
    return directory;
}

} // namespace rc
