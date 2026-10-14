#include "android_file_picker.h"

#include <QJniObject>
#include <QPointer>
#include <QtCore/private/qandroidextras_p.h>

namespace {

constexpr int RequestCode = 2701; // 本程序发起的、要结果的系统界面里，文件选择器是这个
constexpr jint ResultOk = -1;     // Activity.RESULT_OK

QJniObject string(const char *text)
{
    return QJniObject::fromString(QString::fromLatin1(text));
}

QUrl url(const QJniObject &uri)
{
    return QUrl(uri.callObjectMethod("toString", "()Ljava/lang/String;").toString());
}

} // namespace

void AndroidFilePicker::open()
{
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      string("android.intent.action.OPEN_DOCUMENT").object<jstring>());
    intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
                            string("android.intent.category.OPENABLE").object<jstring>());
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", string("*/*").object<jstring>());
    intent.callObjectMethod("putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;",
                            string("android.intent.extra.ALLOW_MULTIPLE").object<jstring>(), jboolean(true));
    // 结果在 Android 的主线程上回来，交回本对象的线程
    QPointer<AndroidFilePicker> self(this);
    QtAndroidPrivate::startActivity(intent, RequestCode, [self](int, int result, const QJniObject &data) {
        if (result != ResultOk || !data.isValid())
            return;
        QList<QUrl> urls;
        // 选了几个时在 ClipData 里，一个时是 getData()
        const QJniObject clip = data.callObjectMethod("getClipData", "()Landroid/content/ClipData;");
        if (clip.isValid()) {
            const jint count = clip.callMethod<jint>("getItemCount");
            for (jint i = 0; i < count; ++i) {
                const QJniObject item = clip.callObjectMethod("getItemAt", "(I)Landroid/content/ClipData$Item;", i);
                urls.append(url(item.callObjectMethod("getUri", "()Landroid/net/Uri;")));
            }
        } else {
            urls.append(url(data.callObjectMethod("getData", "()Landroid/net/Uri;")));
        }
        QMetaObject::invokeMethod(self, [self, urls] {
            if (self)
                emit self->accepted(urls);
        }, Qt::QueuedConnection);
    });
}
