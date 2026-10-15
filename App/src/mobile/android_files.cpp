#include "android_files.h"

#include <QCoreApplication>
#include <QDir>
#include <QJniObject>

namespace android_files {

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

} // namespace android_files
