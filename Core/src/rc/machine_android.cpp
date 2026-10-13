#include "rc/machine.h"

#include <QCoreApplication>
#include <QJniObject>

namespace rc {

// Settings.Secure.ANDROID_ID：同一个签名的程序在这台手机、这个用户下不变，重装程序也不变；恢复出厂设置以后会变
QString machineId()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    return QJniObject::callStaticObjectMethod(
               "android/provider/Settings$Secure", "getString",
               "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;", resolver.object(),
               QJniObject::fromString(QStringLiteral("android_id")).object<jstring>())
        .toString();
}

} // namespace rc
