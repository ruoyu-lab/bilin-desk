#pragma once

#include "setup_platform.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// 安装、卸载的界面（SetupWindow）用的一面。Windows：程序以安装模式启动（Setup.exe 解压以后 --setup install），
// 卸载是应用和功能里的 --setup uninstall；macOS 用 .pkg 安装，卸载在设置里。
// 按 setup_platform 的步骤在后台线程上做，没成就撤回做过的（setup_steps.h）。
// status：ready 等用户点按钮 | busy 正在做（progress 0..1）| done 做完了 | failed 没成（error 说为什么，可以再试）
class Setup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // install | uninstall
    Q_PROPERTY(QString mode READ mode CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // 装到哪（安装时可以改）
    Q_PROPERTY(QString installPath READ installPath NOTIFY changed)
    // 现在运行的这一份是装好的（不是开发版本）：设置里才有"卸载"
    Q_PROPERTY(bool installed READ installed CONSTANT)
    // 安装时桌面上放不放快捷方式（Windows）：第一次装默认放，升级时照旧
    Q_PROPERTY(bool desktopShortcut READ desktopShortcut WRITE setDesktopShortcut NOTIFY changed)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。request 为空：普通启动（卸载用）
    Setup(std::optional<setup_platform::Request> request, QObject *parent);

    static void setInstance(Setup *instance);
    static Setup *create(QQmlEngine *, QJSEngine *);

    QString mode() const;
    QString status() const { return status_; }
    double progress() const { return progress_; }
    QString error() const { return error_; }
    QString installPath() const { return installPath_; }
    bool installed() const { return setup_platform::runningInstalled(); }
    bool desktopShortcut() const { return desktopShortcut_; }
    void setDesktopShortcut(bool desktopShortcut);

    Q_INVOKABLE void start();
    // "更改"里选的文件夹
    Q_INVOKABLE void setFolder(const QUrl &folder);
    // 装好以后打开远程控制
    Q_INVOKABLE void openInstalled();

signals:
    void changed();
    void progressChanged();
    // 卸载完了（macOS：程序随后退出）
    void finished();

private:
    void setProgress(double progress);
    void done(const QString &error);

    const setup_platform::Request request_;
    const QString installedPath_; // 装好的那一份在哪（升级装回那里）；没装过时为空
    QString installPath_;
    bool desktopShortcut_;
    QString status_ = QStringLiteral("ready");
    double progress_ = 0;
    QString error_;
};
