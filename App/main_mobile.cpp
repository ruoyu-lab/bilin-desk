#include "account/account.h"
#include "appearance.h"
#include "chat/conversations.h"
#include "devices/device_directory.h"
#include "devices/device_wake.h"
#include "mobile/local_service.h"
#include "remote_view/view_graphics.h"
#include "service_client.h"
#include "session/viewer_sessions.h"
#include "this_device.h"

#include <rc/interrupted_transfers.h>
#include <rc/ipc/endpoint.h>
#include <rc/log_file.h>
#include <rc/paths.h>

#include <rcsdk/clipboard.h>
#include <rcsdk/file_transfer.h>
#include <rcsdk/power.h>
#include <rcsdk/rcsdk.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

// 手机版（Android、iOS）：只去连电脑，不被连。一个窗口（MobileMain.qml），后台服务在本进程里（LocalService）；
// 没有安装程序、托盘、开机启动、单实例这些电脑上才有的
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Bilin"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("bilindesk.com"));
    QGuiApplication::setApplicationName(QStringLiteral("Bilin"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QQuickStyle::setStyle("Basic");
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

    // 本机身份、账号、信令：在本进程里的后台服务，界面经本机连接和它说话（和电脑上一样）
    LocalService localService(directory, &app);
    if (!localService.start(rc::ipc::serviceEndpoint()))
        qCritical("本机连接听不了：%s", qUtf8Printable(rc::ipc::serviceEndpoint()));
    ServiceClient service(ServiceClient::Runner::InProcess, rc::ipc::serviceEndpoint());
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
    ViewerSessions viewerSessions(service, account, viewGraphics.decodeDevice(), conversations, transfers, &app);
    ViewerSessions::setInstance(&viewerSessions);
    service.start();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Bilin", "MobileMain");
    return QGuiApplication::exec();
}
