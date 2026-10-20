#include "account/account.h"
#include "app_lifecycle.h"
#include "appearance.h"
#include "autostart.h"
#include "chat/conversations.h"
#include "devices/device_directory.h"
#include "devices/device_wake.h"
#include "dock.h"
#include "feedback/feedback.h"
#include "remote_view/view_graphics.h"
#include "service_client.h"
#include "session/viewer_sessions.h"
#include "setup/setup.h"
#include "share/incoming_connections.h"
#include "single_instance.h"
#include "this_device.h"
#include "wallpaper.h"

#include <rc/ipc/endpoint.h>
#include <rc/log_file.h>
#include <rc/paths.h>
#include <rc/system_service.h>

#include <rcsdk/clipboard.h>
#include <rcsdk/file_transfer.h>
#include <rcsdk/power.h>
#include <rcsdk/rcsdk.hpp>

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace {

void loadOrExit(QQmlApplicationEngine &engine, QAnyStringView type)
{
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, qApp, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Bilin", type);
}

// 静默安装、卸载：不要界面，按步骤做完就退出；没成时退出码 1（撤回了做过的），日志里有原因
int runSetupQuietly(setup_platform::Request request)
{
    const bool install = request.mode == setup_platform::Request::Mode::Install;
    const QString installed = setup_platform::installedPath();
    if (install)
        request.target = installed.isEmpty() ? setup_platform::defaultInstallPath() : installed;
    else if (request.target.isEmpty())
        request.target = installed;
    // 桌面上的快捷方式：升级时照旧，第一次装放一个
    const bool desktopShortcut = installed.isEmpty() || setup_platform::hasDesktopShortcut();
    const QString error = runSetupSteps(install ? setup_platform::installSteps(QCoreApplication::applicationDirPath(),
                                                                               request.target, desktopShortcut)
                                                : setup_platform::uninstallSteps(request),
                                        [](double) {});
    if (!install)
        setup_platform::cleanUpAfterExit();
    if (!error.isEmpty()) {
        qCritical("%s没成：%s", install ? "安装" : "卸载", qUtf8Printable(error));
        return 1;
    }
    qInfo("%s好了：%s", install ? "装" : "卸载", qUtf8Printable(request.target));
    return 0;
}

// 安装模式（Windows：Setup.exe 解压以后、应用和功能里点卸载）：只有安装 / 卸载的界面，不连后台服务、没有托盘
int runSetup(const setup_platform::Request &request)
{
    // 要管理员身份、卸载还要先换到临时目录：用新的身份、在新的地方再启动一次，这一次到此为止
    const bool relaunched = request.mode == setup_platform::Request::Mode::Install
                                ? setup_platform::relaunchElevated()
                                : setup_platform::relaunchForUninstall(request);
    if (relaunched)
        return 0;
    rc::openLogFile(QDir::tempPath(), QStringLiteral("BilinSetup.log"));
    if (request.quiet)
        return runSetupQuietly(request);
    Setup setup(request, qApp);
    Setup::setInstance(&setup);
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { setup_platform::cleanUpAfterExit(); });
    QQmlApplicationEngine engine;
    loadOrExit(engine, "SetupWindow");
    // 窗口要自己显示：QML 里没设 visible（和别的窗口一样，是 Main.qml 里创建了再 present）。不显示的话安装程序
    // 一直在运行、却什么都看不到
    for (QObject *root : engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(root)) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    }
    return QGuiApplication::exec();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Bilin"));
    // macOS 的偏好设置按它存：~/Library/Preferences/com.bilindesk.Bilin.plist（卸载时删，setup_mac.mm）
    QGuiApplication::setOrganizationDomain(QStringLiteral("bilindesk.com"));
    QGuiApplication::setApplicationName(QStringLiteral("Bilin"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    // 浅色、深色：在任何窗口之前设好（安装界面也跟着）
    Appearance appearance(&app);
    Appearance::setInstance(&appearance);
    if (const auto request = setup_platform::requestFromArguments(app.arguments()))
        return runSetup(*request);

    // 已经有一个在运行：让它把窗口拿出来，自己退出
    SingleInstance single;
    if (single.forwardToRunning())
        return 0;
    // 日志：状态目录的 app.log（排查时和后台服务的 service.log 一起看）
    rc::openLogFile(rc::stateDirectory(), QStringLiteral("app.log"));
    // SDK 的日志（去连别人的会话）也进 app.log
    rcsdk::setLogCallback(RCS_LOG_INFO, [](const rcs_log_record &record) {
        if (record.level <= RCS_LOG_WARNING)
            qWarning("[sdk %s] %s", record.module, record.message);
        else
            qInfo("[sdk %s] %s", record.module, record.message);
    });
    // SDK 的模块在任何会话之前注册：剪贴板、互传文件
    rcsdk::registerModule(*rcs_clipboard_module());
    rcsdk::registerModule(*rcs_file_transfer_module());
    rcsdk::registerModule(*rcs_power_module());
    // 关掉窗口不退出：收到托盘 / 菜单栏
    QGuiApplication::setQuitOnLastWindowClosed(false);
    AppLifecycle lifecycle(&app);
    AppLifecycle::setInstance(&lifecycle);
    QObject::connect(&single, &SingleInstance::activated, &lifecycle, &AppLifecycle::showRequested);
    installDockReopenHandler([&lifecycle] { emit lifecycle.showRequested(); });
    // 开机时自动启动（系统的登录项）：登录时由系统启动的只在托盘 / 菜单栏，不弹窗口（Main.qml）
    Autostart autostart(&app);
    Autostart::setInstance(&autostart);

    // 本机的身份和被控那一面在后台服务里：连上它；开发版没在运行就启动它。装好的那一份连系统服务；
    // 这台电脑装了比邻、却是别处编出来的开发版时也连旁边的后台服务（同一次编译，连开发服务器）
    const bool systemService = rc::system_service::installed() && setup_platform::runningInstalled();
    ServiceClient service(systemService ? ServiceClient::Runner::System : ServiceClient::Runner::Beside,
                          systemService ? rc::system_service::endpoint() : rc::ipc::serviceEndpoint());
    ThisDevice thisDevice(service, &app);
    ThisDevice::setInstance(&thisDevice);
    // 有人连进本机时把本机桌面的背景给他（后台服务要）
    Wallpaper wallpaper(service, &app);
    // 装好了系统服务时，开机时自动启动的开关也管它在界面程序没开着时接不接受我的设备连接
    if (service.systemService()) {
        autostart.followService([&thisDevice] { return thisDevice.unattended(); },
                                [&thisDevice](bool unattended) { thisDevice.setUnattended(unattended); });
        QObject::connect(&thisDevice, &ThisDevice::changed, &autostart, &Autostart::changed);
    }
    // 登录的账号：同一个账号下的设备互为"我的设备"
    Account account(service, &app);
    Account::setInstance(&account);
    // 反馈：带上界面程序的日志（这次和上次运行的；macOS 装好时还有登录的人的助手的），后台服务的由它自己带
    QStringList feedbackLogs{rc::stateDirectory() + QStringLiteral("/app.log"),
                             rc::stateDirectory() + QStringLiteral("/app.log.previous")};
#ifdef Q_OS_MACOS
    const QString agentLog = QDir::homePath() + QStringLiteral("/Library/Logs/Bilin/agent.log");
    feedbackLogs << agentLog << agentLog + QStringLiteral(".previous");
#endif
    Feedback feedback(service, feedbackLogs, &app);
    Feedback::setInstance(&feedback);
    // 设备列表：我的设备（账号下的）、附近的（后台服务发现的）和本机记着的（添加的、收藏的、连过的）
    DeviceDirectory devices(service, account, rc::stateDirectory() + QStringLiteral("/devices.json"), &app);
    DeviceDirectory::setInstance(&devices);
    // 网络唤醒：我的设备说过的网卡记在状态目录的 wake.json，它睡眠、关机以后从本机叫醒；本机叫不醒时请账号下
    // 开着的别的电脑帮忙（经后台服务、服务器）
    DeviceWake wake(service, account, rc::stateDirectory() + QStringLiteral("/wake.json"), &app);
    DeviceWake::setInstance(&wake);
    // 和各台设备的对话：不管谁连谁，同一台设备是同一份，记录存在本机（状态目录的 chats）
    Conversations conversations(rc::stateDirectory() + QStringLiteral("/chats"), &app);
    // 别人连本机：请求和连着的人在后台服务里，界面上显示、决定
    IncomingConnections incoming(service, conversations, &app);
    IncomingConnections::setInstance(&incoming);
    // 有人连着本机时用户要退出：先问（AppLifecycle.quitHeld）。真要退了先告诉后台服务，
    // 它断开连着本机的人，之后界面没了不再把它拉起来
    QObject::connect(&incoming, &IncomingConnections::changed, &lifecycle,
                     [&] { lifecycle.setHoldQuit(!incoming.viewers().isEmpty()); });
    QObject::connect(&lifecycle, &AppLifecycle::quittingChanged, &service, [&] {
        if (lifecycle.quitting())
            service.appQuitting();
    });
    // 去连别人：观看会话在界面程序里，信令经后台服务；画面在会话窗口里画（图形设置要在任何窗口之前）
    ViewGraphics viewGraphics(&app);
    ViewGraphics::setInstance(&viewGraphics);
    // 去连别人时没传完的文件（下次接着传）
    rc::InterruptedTransfers transfers(rc::stateDirectory() + QStringLiteral("/viewer-transfers.json"));
    // 本机的解码器：一开始就在后台列（第一次要把编解码器挨个试一遍），去连的时候就不用等
    LocalCodecs codecs(&app);
    ViewerSessions viewerSessions(service, account, viewGraphics.decodeDevice(), codecs, conversations, transfers,
                                  &app);
    ViewerSessions::setInstance(&viewerSessions);
    QObject::connect(&viewerSessions, &ViewerSessions::wallpaperReceived, &devices, &DeviceDirectory::setWallpaper);
    // 卸载（macOS 在设置里）
    Setup setup(std::nullopt, &app);
    Setup::setInstance(&setup);
    service.start();

    QQmlApplicationEngine engine;
    loadOrExit(engine, "Main");

    return QGuiApplication::exec();
}
