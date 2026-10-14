#pragma once

#include <QList>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Android：系统的文件选择器（Intent.ACTION_OPEN_DOCUMENT，能多选），选好的是 content:// 地址（qml/android/FilePickers.qml）。
// QtQuick.Dialogs 的 FileDialog 在 Android 上也是调它，但会把 Qt 自己画的各种风格的对话框一起打进 APK（二十多 MB）
class AndroidFilePicker : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    using QObject::QObject;

    Q_INVOKABLE void open();

signals:
    void accepted(const QList<QUrl> &urls);
};
