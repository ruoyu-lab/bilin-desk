#include "account/account.h"
#include "appearance.h"
#include "chat/conversations.h"
#include "devices/device_directory.h"
#include "devices/device_wake.h"
#include "mobile/local_service.h"
#include "mobile/mobile_files.h"
#include "mobile/phone_system.h"
#include "remote_view/view_graphics.h"
#include "service_client.h"
#include "session/viewer_sessions.h"
#include "this_device.h"

#include <rc/interrupted_transfers.h>
#include <rc/log_file.h>
#include <rc/paths.h>

#include <rcsdk/clipboard.h>
#include <rcsdk/file_transfer.h>
#include <rcsdk/power.h>
#include <rcsdk/rcsdk.hpp>
#ifdef Q_OS_ANDROID
#include "mobile/session_keeper.h"

#include <rcsdk/android.h>

#include <QJniEnvironment>
#include <QJniObject>
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>

// 手机版（Android、iOS）：只去连电脑，不被连。一个窗口（MobileMain.qml），后台服务在本进程里（LocalService）；
// 没有安装程序、托盘、开机启动、单实例这些电脑上才有的
int main(int argc, char *argv[])
{
    // 平板是电脑版的布局，整体放大一些（拿在手里离眼睛远，电脑上的大小看着费劲）。只能在建 QGuiApplication 之前定
    const bool tablet = phone_system::tablet();
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    if (tablet && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
        qputenv("QT_SCALE_FACTOR", "1.15");
#endif
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Bilin"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("bilindesk.com"));
    QGuiApplication::setApplicationName(QStringLiteral("Bilin"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
#ifdef Q_OS_ANDROID
    // SDK 用到 Java 的地方（网络变化、硬件缓冲区……）要本进程的 Java 虚拟机和程序的 Context
    rcs_android_initialize(QJniEnvironment::javaVM(), QNativeInterface::QAndroidApplication::context().object());
#endif
    Appearance appearance(&app);
    Appearance::setInstance(&appearance);

    const QString directory = rc::stateDirectory();
    rc::openLogFile(directory, QStringLiteral("app.log"));
    rcsdk::setLogCallback(RCS_LOG_INFO, [](const rcs_log_record &record) {
        if (record.level <= RCS_LOG_WARNING)
            qWarning("[sdk %s] %s", record.module, record.message);
        else
            qInfo("[sdk %s] %s", record.module, record.message);
    });
    // SDK 的模块在任何会话之前注册：剪贴板、互传文件、对方电脑的电源
    rcsdk::registerModule(*rcs_clipboard_module());
    rcsdk::registerModule(*rcs_file_transfer_module());
    rcsdk::registerModule(*rcs_power_module());
    // 本机的解码器：一开始就在后台列（Android 上第一次要把编解码器挨个试一遍，好几秒），去连的时候就不用等
    LocalCodecs codecs(&app);

    // 本机身份、账号、信令：在本进程里的后台服务，界面经本机连接和它说话（和电脑上一样）。连接只在本程序里用
    //（沙盒里只有它自己），名字短：它是程序临时目录里的一个文件，iOS 上那个目录的路径就有 80 多个字节，
    // 整个路径不能超过 104 个字节
    const QString endpoint = QStringLiteral("bilin-service");
    LocalService localService(directory, &app);
    if (!localService.start(endpoint))
        qCritical("本机连接听不了：%s", qUtf8Printable(endpoint));
    ServiceClient service(ServiceClient::Runner::InProcess, endpoint);
    ThisDevice thisDevice(service, &app);
    ThisDevice::setInstance(&thisDevice);
    Account account(service, &app);
    Account::setInstance(&account);
    DeviceDirectory devices(service, account, directory + QStringLiteral("/devices.json"), &app);
    DeviceDirectory::setInstance(&devices);
    DeviceWake wake(directory + QStringLiteral("/wake.json"), &app);
    DeviceWake::setInstance(&wake);
    Conversations conversations(directory + QStringLiteral("/chats"), &app);
    ViewGraphics viewGraphics(&app);
    ViewGraphics::setInstance(&viewGraphics);
    rc::InterruptedTransfers transfers(directory + QStringLiteral("/viewer-transfers.json"));
    // 上次为发送复制出来、已经不用接着发的文件删掉
    mobile_files::removeStaleCopies(transfers);
    ViewerSessions viewerSessions(service, account, viewGraphics.decodeDevice(), codecs, conversations, transfers,
                                  &app);
    ViewerSessions::setInstance(&viewerSessions);
    QObject::connect(&viewerSessions, &ViewerSessions::wallpaperReceived, &devices, &DeviceDirectory::setWallpaper);
#ifdef Q_OS_ANDROID
    SessionKeeper keeper(viewerSessions, &app);
#endif
    service.start();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Bilin", tablet ? "TabletMain" : "MobileMain");
    return QGuiApplication::exec();
}
