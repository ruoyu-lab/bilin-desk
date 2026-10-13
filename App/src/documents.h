#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QString>
#include <QtQml/qqmlregistration.h>

// 程序里带着的文字：开源声明里的许可全文（:/licenses/）、服务协议和隐私政策（:/legal/）。
// 文件编进程序里（App/CMakeLists.txt 里的 licenses、legal）
class Documents : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // path：资源里的路径，比如 "licenses/LGPL-3.0.txt"、"legal/privacy.md"
    Q_INVOKABLE QString text(const QString &path) const;
    // 读进来的 Markdown 排一下版：QTextDocument 读 Markdown 时行距、段落之间、标题前后都不留白，挤在一起。
    // 正文的行距放宽、段落之间空开，标题比正文大一点、前面多空一些，列表的各项之间空一点
    Q_INVOKABLE void typeset(QQuickTextDocument *document) const;
};
