#include "android_files.h"

#include <rc/interrupted_transfers.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJniObject>
#include <QStandardPaths>

#include <algorithm>

namespace android_files {

namespace {

// 复制出来要发的文件放在缓存的这里，一次一个文件夹（同名的不撞）
QString copies()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/sending");
}

} // namespace

QString downloads()
{
    constexpr jint AndroidR = 30;
    const jint sdk = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
    const QJniObject name = QJniObject::fromString(QStringLiteral("Download"));
    QString directory;
    if (sdk >= AndroidR) {
        const QJniObject folder = QJniObject::callStaticObjectMethod(
            "android/os/Environment", "getExternalStoragePublicDirectory", "(Ljava/lang/String;)Ljava/io/File;",
            name.object<jstring>());
        directory = folder.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString()
                    + QStringLiteral("/比邻");
    } else {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        const QJniObject folder =
            context.callObjectMethod("getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;", name.object<jstring>());
        directory = folder.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString();
    }
    QDir().mkpath(directory);
    return directory;
}

QString copyForSending(const QUrl &url)
{
    // Qt 认 content:// 地址（QFile、QFileInfo），名字是文件选择器里看到的
    const QString source = url.toString();
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

} // namespace android_files
