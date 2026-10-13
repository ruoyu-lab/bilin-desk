#include "system_mode.h"

#include <rc/programs.h>
#include <rc/system_service.h>

#include <rcsdk/rcsdk.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSocketNotifier>

#include <SystemConfiguration/SystemConfiguration.h>

#include <csignal>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace system_mode {

namespace {

// path 不只有 root 能动时为什么；secret：别人也不能读
QString protectionProblem(const QString &path, bool secret)
{
    struct stat info{};
    if (lstat(QFile::encodeName(path).constData(), &info) != 0)
        return QStringLiteral("%1 读不出来（%2）").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
    if (S_ISLNK(info.st_mode))
        return QStringLiteral("%1 是符号链接").arg(path);
    if (info.st_uid != 0)
        return QStringLiteral("%1 不属于 root").arg(path);
    if (info.st_mode & (S_IWGRP | S_IWOTH))
        return QStringLiteral("%1 别的账户能改").arg(path);
    if (secret && (info.st_mode & (S_IRWXG | S_IRWXO)))
        return QStringLiteral("%1 别的账户能读").arg(path);
    return {};
}

uid_t consoleUser()
{
    uid_t uid = 0;
    if (CFStringRef name = SCDynamicStoreCopyConsoleUser(nullptr, &uid, nullptr))
        CFRelease(name);
    return uid;
}

int stopSignal[2] = {-1, -1};

} // namespace

QString prepareStateDirectory()
{
    // 本机连接、助手的链接的套接字放在 /var/run 下（开机时清空，每次建）：每个用户都要进得去
    const QString run = QFileInfo(rc::system_service::endpoint()).absolutePath();
    if (!QDir().mkpath(run) || ::chmod(QFile::encodeName(run).constData(), 0755) != 0)
        return QStringLiteral("建不了 %1").arg(run);
    // 上一层放助手也要读的链接密钥（agentKey），数据目录本身只有 root 能进
    const QString directory = rc::system_service::stateDirectory();
    const QString parent = QFileInfo(directory).absolutePath();
    if (!QDir().mkpath(directory))
        return QStringLiteral("建不了 %1").arg(directory);
    if (::chmod(QFile::encodeName(parent).constData(), 0755) != 0
        || ::chmod(QFile::encodeName(directory).constData(), 0700) != 0)
        return QStringLiteral("改不了 %1 的权限").arg(directory);
    if (const QString problem = protectionProblem(parent, false); !problem.isEmpty())
        return problem;
    return protectionProblem(directory, true);
}

QString programProblem()
{
    const QString program = QCoreApplication::applicationFilePath();
    if (const QString problem = protectionProblem(program, false); !problem.isEmpty())
        return problem;
    return protectionProblem(QFileInfo(program).absolutePath(), false);
}

AppKeeper::Launch launchApp()
{
    const uid_t user = consoleUser();
    if (user == 0)
        return AppKeeper::Launch::NobodyThere;
    // 后台服务在 .app 的 Contents/Helpers 里
    const QString bundle = QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).canonicalPath();
    const bool started = QProcess::startDetached(QStringLiteral("/bin/launchctl"),
                                                 {QStringLiteral("asuser"), QString::number(user),
                                                  QStringLiteral("/usr/bin/open"), QStringLiteral("-g"),
                                                  QStringLiteral("-j"), QStringLiteral("-a"), bundle,
                                                  QStringLiteral("--args"), rc::program::BackgroundArgument});
    if (!started)
        qWarning("给控制台前的用户启动界面程序没成");
    return started ? AppKeeper::Launch::Started : AppKeeper::Launch::Failed;
}

void onStopRequest(std::function<void()> stop)
{
    // 信号处理里只能做很少的事：写一个字节，主线程上读到了再停
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, stopSignal) != 0) {
        qWarning("收不到 launchd 的结束信号");
        return;
    }
    std::signal(SIGTERM, [](int) {
        const char byte = 1;
        [[maybe_unused]] const ssize_t written = ::write(stopSignal[0], &byte, 1);
    });
    auto *notifier = new QSocketNotifier(stopSignal[1], QSocketNotifier::Read, QCoreApplication::instance());
    QObject::connect(notifier, &QSocketNotifier::activated, notifier, [notifier, stop = std::move(stop)] {
        notifier->setEnabled(false);
        stop();
    });
}

// 安装程序是 launchd 里单独的一个任务：后台服务被安装包的 preinstall 停掉时它接着跑，输出记在数据目录里
bool installUpdate()
{
    const QString label = QStringLiteral("com.bilindesk.bilin.update");
    const QString directory = rc::system_service::stateDirectory();
    const QString plist = directory + QStringLiteral("/update.plist");
    // 上次更新的任务（早就跑完了）还在 launchd 里：先去掉
    QProcess::execute(QStringLiteral("/bin/launchctl"), {QStringLiteral("bootout"), QStringLiteral("system/") + label});
    QFile file(plist);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const auto string = [](const QString &text) { return QStringLiteral("<string>%1</string>").arg(text.toHtmlEscaped()); };
    file.write(QStringLiteral(
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\"><dict>"
                   "<key>Label</key>%1"
                   "<key>ProgramArguments</key><array>%2%3%4%5%6</array>"
                   "<key>RunAtLoad</key><true/>"
                   "<key>StandardOutPath</key>%7<key>StandardErrorPath</key>%7"
                   "</dict></plist>\n")
                   .arg(string(label), string(QStringLiteral("/usr/sbin/installer")), string(QStringLiteral("-pkg")),
                        string(rc::system_service::updateInstaller()), string(QStringLiteral("-target")),
                        string(QStringLiteral("/")), string(directory + QStringLiteral("/update-install.log")))
                   .toUtf8());
    file.close();
    return QProcess::execute(QStringLiteral("/bin/launchctl"), {QStringLiteral("bootstrap"), QStringLiteral("system"), plist})
           == 0;
}

QByteArray agentKey()
{
    const QString path = rc::system_service::agentKey();
    QFile file(path);
    if (file.exists()) {
        if (const QString problem = protectionProblem(path, false); !problem.isEmpty()) {
            qCritical("助手的链接密钥没保护好：%s", qUtf8Printable(problem));
            return {};
        }
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.read(32);
    }
    const rcsdk::ConnectionKey key = rcsdk::generateConnectionKey();
    const QByteArray bytes(reinterpret_cast<const char *>(key.data()), qsizetype(key.size()));
    // 每个用户的助手都要读得到（它以登录的那个用户运行）；拿着它也只能过链接的握手，认不认还看 AgentGate
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(bytes) != bytes.size()) {
        qCritical("写不了助手的链接密钥 %s", qUtf8Printable(path));
        return {};
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return bytes;
}

} // namespace system_mode
