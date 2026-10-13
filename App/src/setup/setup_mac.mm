#include "setup_platform.h"

#include "login_item.h"

#include <rc/paths.h>
#include <rc/system_service.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>
#include <unistd.h>

namespace setup_platform {

namespace {

// .pkg 装到这里（scripts/build-macos-pkg.sh）
constexpr QLatin1StringView InstalledBundle("/Applications/Bilin.app");
// .pkg 的标识：卸载时让系统忘掉这次安装
constexpr QLatin1StringView PackageIdentifier("com.bilindesk.bilin");
// 偏好设置（QSettings）的域
constexpr QLatin1StringView PreferencesDomain("com.bilindesk.Bilin");

QString runningBundle()
{
    return QString::fromNSString(NSBundle.mainBundle.bundlePath);
}

// 'shell 里的一个参数'
QString shellQuoted(const QString &text)
{
    QString quoted = text;
    quoted.replace(u'\'', QStringLiteral("'\\''"));
    return u'\'' + quoted + u'\'';
}

// AppleScript 的字符串字面量
QString appleScriptString(const QString &text)
{
    QString escaped = text;
    escaped.replace(u'\\', QStringLiteral("\\\\"));
    escaped.replace(u'"', QStringLiteral("\\\""));
    return u'"' + escaped + u'"';
}

// 用管理员身份跑一段 shell：系统弹窗要管理员密码
QString runAsAdministrator(const QString &script)
{
    QProcess process;
    process.start(QStringLiteral("/usr/bin/osascript"),
                  {QStringLiteral("-e"),
                   QStringLiteral("do shell script %1 with administrator privileges").arg(appleScriptString(script))});
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit)
        return QCoreApplication::translate("Setup", "卸载没做完");
    if (process.exitCode() == 0)
        return {};
    // -128：用户点了取消
    const QString output = QString::fromUtf8(process.readAllStandardError());
    if (output.contains(QStringLiteral("-128")))
        return QCoreApplication::translate("Setup", "没有输入管理员密码，没卸载");
    qWarning("卸载：%s", qUtf8Printable(output.trimmed()));
    return QCoreApplication::translate("Setup", "卸载没做完");
}

} // namespace

QString installedPath()
{
    return QFileInfo::exists(InstalledBundle) ? QString(InstalledBundle) : QString();
}

bool runningInstalled()
{
    return runningBundle() == InstalledBundle;
}

QString defaultInstallPath()
{
    return InstalledBundle;
}

bool relaunchElevated()
{
    return false;
}

bool relaunchForUninstall(const Request &)
{
    return false;
}

bool hasDesktopShortcut()
{
    return false;
}

std::vector<SetupStep> installSteps(const QString &, const QString &, bool)
{
    // macOS 用 .pkg 安装（系统的安装器），不经这里
    return {};
}

std::vector<SetupStep> uninstallSteps(const Request &request)
{
    const QString bundle = request.target;
    const QString data = rc::stateDirectory();
    return {
        // 登录项：SMAppService 在主线程上改
        {QStringLiteral("关掉开机时自动启动"), 1, [](const auto &) {
             dispatch_sync(dispatch_get_main_queue(), ^{
                 login_item::setEnabled(false);
             });
             return QString();
         }, {}},
        // 程序在"应用程序"里，删它要管理员；开机就运行的后台服务、各个会话里的助手（rc/system_service.h）先停掉、
        // 去掉，整台电脑那一份数据也删掉
        {QStringLiteral("删除程序"), 6, [bundle](const auto &) {
             const QString daemon = QLatin1StringView(rc::system_service::DaemonLabel);
             const QString agent = QLatin1StringView(rc::system_service::AgentLabel);
             return runAsAdministrator(
                 QStringLiteral("/bin/launchctl bootout system/%1 2>/dev/null; "
                                "/bin/launchctl bootout gui/%2/%3 2>/dev/null; "
                                "/usr/bin/killall BilinService BilinAgent 2>/dev/null; "
                                "/bin/rm -f %4 %5; "
                                "/bin/rm -rf %6 %7 %8; "
                                "/usr/sbin/pkgutil --forget %9 >/dev/null 2>&1; true")
                     .arg(daemon, QString::number(getuid()), agent,
                          shellQuoted(rc::system_service::daemonPlist()), shellQuoted(rc::system_service::agentPlist()),
                          shellQuoted(bundle),
                          shellQuoted(QFileInfo(rc::system_service::agentKey()).absolutePath()),
                          shellQuoted(QFileInfo(rc::system_service::endpoint()).absolutePath()),
                          QString(PackageIdentifier)));
         }, {}},
        {QStringLiteral("删除数据"), 1, [data](const auto &) {
             QProcess::execute(QStringLiteral("/usr/bin/defaults"), {QStringLiteral("delete"), PreferencesDomain});
             if (QFileInfo::exists(data) && !QDir(data).removeRecursively())
                 return QCoreApplication::translate("Setup", "删不掉 %1").arg(data);
             return QString();
         }, {}},
    };
}

void openInstalled(const QString &target)
{
    QProcess::startDetached(QStringLiteral("/usr/bin/open"), {target});
}

void cleanUpAfterExit()
{
}

} // namespace setup_platform
