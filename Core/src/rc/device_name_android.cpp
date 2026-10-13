#include "rc/device_name.h"

#include <QCoreApplication>
#include <QJniObject>

namespace rc {

QString defaultDeviceName()
{
    // "设置 › 关于手机"里的名字（Settings.Global.DEVICE_NAME）；有的系统没有这一项，用机型
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    const QString name = QJniObject::callStaticObjectMethod(
                             "android/provider/Settings$Global", "getString",
                             "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                             resolver.object(), QJniObject::fromString(QStringLiteral("device_name")).object<jstring>())
                             .toString();
    if (!name.isEmpty())
        return name;
    return QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL").toString();
}

} // namespace rc
