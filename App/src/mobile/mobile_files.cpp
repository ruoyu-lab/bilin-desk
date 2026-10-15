#include "mobile_files.h"

#include <rc/interrupted_transfers.h>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace mobile_files {

namespace {

// 复制出来要发的文件放在缓存的这里，一次一个文件夹（同名的不撞）
QString copies()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/sending");
}

} // namespace

QString copyForSending(const QUrl &url)
{
    const QString source = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QString directory =
        copies() + u'/' + QString::number(QDateTime::currentMSecsSinceEpoch()) + u'-' + QString::number(qHash(source));
    if (!QDir().mkpath(directory))
        return {};
    const QString target = directory + u'/' + QFileInfo(source).fileName();
    if (!QFile::copy(source, target)) {
        qWarning("要发送的文件复制不了：%s", qUtf8Printable(source));
        return {};
    }
    return target;
}

void removeStaleCopies(const rc::InterruptedTransfers &interrupted)
{
    const QStringList keep = interrupted.sendingPaths();
    QDirIterator folders(copies(), QDir::Dirs | QDir::NoDotAndDotDot);
    while (folders.hasNext()) {
        const QString folder = folders.next();
        const bool wanted = std::any_of(keep.cbegin(), keep.cend(),
                                        [&folder](const QString &path) { return path.startsWith(folder + u'/'); });
        if (!wanted)
            QDir(folder).removeRecursively();
    }
}

} // namespace mobile_files
