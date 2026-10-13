#include "app_keeper.h"

#include <rc/programs.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

AppKeeper::AppKeeper(std::function<Launch()> launch, std::function<void()> disconnectAll, int waitMs, QObject *parent)
    : QObject(parent)
    , launch_(std::move(launch))
    , disconnectAll_(std::move(disconnectAll))
{
    wait_.setSingleShot(true);
    wait_.setInterval(waitMs);
    connect(&wait_, &QTimer::timeout, this, &AppKeeper::gaveUp);
}

void AppKeeper::setAttended(bool attended)
{
    attended_ = attended;
    if (attended_) {
        quitting_ = false;
        wait_.stop();
    }
    check();
}

void AppKeeper::setWatched(bool watched)
{
    watched_ = watched;
    if (!watched_)
        wait_.stop();
    check();
}

void AppKeeper::appQuitting()
{
    quitting_ = true;
    if (!watched_)
        return;
    qInfo("界面程序要退出了：断开连着本机的人");
    disconnectAll_();
}

void AppKeeper::check()
{
    if (attended_ || !watched_ || quitting_ || wait_.isActive())
        return;
    switch (launch_()) {
    case Launch::Started:
        qWarning("有人连着本机，界面程序却不在了：把它拉起来");
        wait_.start();
        break;
    case Launch::Failed:
        gaveUp();
        break;
    case Launch::NobodyThere:
        qInfo("有人连着本机，电脑前没人登录：不用界面程序");
        break;
    }
}

void AppKeeper::gaveUp()
{
    if (attended_ || !watched_)
        return;
    qWarning("界面程序没起来，本机的人看不到有人连着：断开连着本机的人");
    disconnectAll_();
}

AppKeeper::Launch launchAppBeside()
{
    const QString program = QStandardPaths::findExecutable(rc::program::App,
                                                           {rc::program::appDirectory(QCoreApplication::applicationDirPath())});
    if (program.isEmpty()) {
        qWarning("后台服务旁边没有界面程序（%s）", rc::program::App.data());
        return AppKeeper::Launch::Failed;
    }
    if (!QProcess::startDetached(program, {rc::program::BackgroundArgument}, QFileInfo(program).absolutePath())) {
        qWarning("界面程序启动不了：%s", qUtf8Printable(program));
        return AppKeeper::Launch::Failed;
    }
    return AppKeeper::Launch::Started;
}
