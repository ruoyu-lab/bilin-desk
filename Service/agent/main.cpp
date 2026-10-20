// macOS 装好时的助手（rc/system_service.h、docs/install-and-first-run.md）：LaunchAgent 在登录窗口（root）和每个用户的
// 会话里各运行一个。后台服务（LaunchDaemon）不在任何窗口服务器会话里，录不了屏：它的两个分享会话（我的设备、别人）的
// 录屏、声音、键鼠、剪贴板、读写文件都在这里做（SDK 的 agent），以这个用户自己的权限。
// 连到两个会话的链接上；链接断了、后台服务重启了就再连。后台服务只认控制台前的那一个（agent_gate.h）。
// 授权（屏幕录制、辅助功能）是这个程序的：用户的会话里没有时弹出系统的提示，引导页带用户去系统设置里打开。
// 后台服务只在连上时听助手说一次有没有授权，本进程也只看得到启动时的授权：授权变了本程序就退出，launchd 再启动它，
// 重新连上去
#include <rc/log_file.h>
#include <rc/system_service.h>
#include <rc/transport.h>

#include <rcsdk/agent.hpp>
#include <rcsdk/clipboard.h>
#include <rcsdk/file_transfer.h>
#include <rcsdk/power.h>
#include <rcsdk/thumbnail.h>
#include <rcsdk/rcsdk.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSocketNotifier>
#include <QTimer>

#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

int stopSignal[2] = {-1, -1};

// 缺授权时多久看一次
constexpr int PermissionPollMs = 3000;
// 本程序带着它启动时只说有哪些授权（退出码，permissions()）
constexpr char PermissionsArgument[] = "--permissions";
constexpr int ScreenGranted = 1;
constexpr int InputGranted = 2;

// 系统给每个进程只查一次授权（屏幕录制、辅助功能都是），之后问的都是那一次的结果
int permissions()
{
    return (rcsdk::screenCapturePermission() == RCS_PERMISSION_GRANTED ? ScreenGranted : 0)
           | (rcsdk::inputPermission() == RCS_PERMISSION_GRANTED ? InputGranted : 0);
}

const char *stateName(rcs_state state)
{
    switch (state) {
    case RCS_STATE_CONNECTING: return "在找后台服务";
    case RCS_STATE_RUNNING: return "连上了";
    case RCS_STATE_RECONNECTING: return "断了，再连";
    case RCS_STATE_STOPPED: return "停了";
    case RCS_STATE_FAILED: return "出错了";
    default: return "?";
    }
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc == 2 && qstrcmp(argv[1], PermissionsArgument) == 0)
        return permissions();

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Bilin"));
    QCoreApplication::setApplicationName(QStringLiteral("BilinAgent"));
    // 登录窗口上（root）记在后台服务的数据目录里，用户的会话里记在他自己的 ~/Library/Logs
    const bool loginWindow = geteuid() == 0;
    rc::openLogFile(loginWindow ? rc::system_service::stateDirectory()
                                : QDir::homePath() + QStringLiteral("/Library/Logs/Bilin"),
                    loginWindow ? QStringLiteral("agent-loginwindow.log") : QStringLiteral("agent.log"));
    rcsdk::setLogCallback(RCS_LOG_INFO, [](const rcs_log_record &record) {
        if (record.level <= RCS_LOG_WARNING)
            qWarning("[sdk %s] %s", record.module, record.message);
        else
            qInfo("[sdk %s] %s", record.module, record.message);
    });
    // 会话用的模块，机器那一半在这里跑
    rcsdk::registerModule(*rcs_clipboard_module());
    rcsdk::registerModule(*rcs_file_transfer_module());
    rcsdk::registerModule(*rcs_power_module());
    rcsdk::registerModule(*rcs_thumbnail_module());

    // 用户的会话里：没授权时弹出系统的提示（只有第一次会弹），把本程序加进系统设置的列表里。
    // 然后定时另启动一个本程序去看（本进程看到的一直是启动时的），变了就退出
    const int granted = permissions();
    QTimer permissionTimer;
    QProcess permissionCheck;
    if (!loginWindow && granted != (ScreenGranted | InputGranted)) {
        if (!(granted & ScreenGranted))
            rcsdk::requestScreenCapturePermission();
        if (!(granted & InputGranted))
            rcsdk::requestInputPermission();
        permissionCheck.setProgram(QCoreApplication::applicationFilePath());
        permissionCheck.setArguments({QLatin1StringView(PermissionsArgument)});
        QObject::connect(&permissionCheck, &QProcess::finished, &app, [granted](int now, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || now == granted)
                return;
            qInfo("授权变了（屏幕录制%s，辅助功能%s），重新启动助手，好让后台服务知道",
                  now & ScreenGranted ? "有" : "没有", now & InputGranted ? "有" : "没有");
            QCoreApplication::quit();
        });
        permissionTimer.setInterval(PermissionPollMs);
        QObject::connect(&permissionTimer, &QTimer::timeout, &app, [&permissionCheck] {
            if (permissionCheck.state() == QProcess::NotRunning)
                permissionCheck.start();
        });
        permissionTimer.start();
    }

    QFile keyFile(rc::system_service::agentKey());
    rcsdk::ConnectionKey key{};
    if (!keyFile.open(QIODevice::ReadOnly) || keyFile.read(reinterpret_cast<char *>(key.data()), qint64(key.size()))
                                                  != qint64(key.size())) {
        // 后台服务还没起来、还没生成：launchd 过一会儿再启动本程序
        qWarning("读不到链接的密钥 %s", qUtf8Printable(keyFile.fileName()));
        return 1;
    }

    std::vector<std::unique_ptr<rcsdk::Agent>> agents;
    // 后台服务的三个分享会话各有一条链接（Sharing）：别人、我的设备、网页版的我的设备（只在浏览器来连时开着）
    for (const QLatin1StringView access : {rc::PasswordAccess, rc::DeviceAccess, rc::WebKeyAccess}) {
        rcsdk::AgentConfig config(rc::system_service::agentEndpoint(access).toStdString(), key);
        config.setReconnect(true, 2000);
        const QString name = access == rc::DeviceAccess   ? QStringLiteral("我的设备")
                             : access == rc::WebKeyAccess ? QStringLiteral("网页版的我的设备")
                                                          : QStringLiteral("别人");
        agents.push_back(std::make_unique<rcsdk::Agent>(config, [name](const rcs_agent_event &event) {
            qInfo("%s的分享会话：%s（%s）", qUtf8Printable(name), stateName(event.state.value),
                  rcs_result_message(event.state.result));
        }));
        if (agents.back()->start() != RCS_OK)
            qCritical("连不上%s的分享会话：%s", qUtf8Printable(name), rcsdk::lastCallError().c_str());
    }

    // launchd 用 SIGTERM 结束它：信号处理里只写一个字节，主线程上读到了再停
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, stopSignal) == 0) {
        std::signal(SIGTERM, [](int) {
            const char byte = 1;
            [[maybe_unused]] const ssize_t written = ::write(stopSignal[0], &byte, 1);
        });
        auto *notifier = new QSocketNotifier(stopSignal[1], QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app, &QCoreApplication::quit);
    }
    const int code = QCoreApplication::exec();
    for (const auto &agent : agents)
        agent->stop();
    return code;
}
