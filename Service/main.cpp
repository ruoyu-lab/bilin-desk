#include "app_keeper.h"
#include "connection_history.h"
#include "control_server.h"
#include "device_store.h"
#include "feedback_sender.h"
#include "relay_channel.h"
#include "lan_discovery.h"
#include "lan_signaling.h"
#include "server_link.h"
#include "service_wiring.h"
#include "share_chat.h"
#include "sharing.h"
#include "signaling.h"
#include "system_mode.h"
#include "updater.h"
#include "wake_helper.h"

#include <rc/ipc/endpoint.h>
#include <rc/log_file.h>
#include <rc/paths.h>
#include <rc/peer_protocol.h>
#include <rc/programs.h>
#include <rc/release.h>
#include <rc/system_service.h>

#include <rcsdk/clipboard.h>
#include <rcsdk/file_transfer.h>
#include <rcsdk/power.h>
#include <rcsdk/thumbnail.h>
#include <rcsdk/rcsdk.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLockFile>
#include <QNetworkInterface>
#include <QVersionNumber>

#include <algorithm>
#include <array>
#include <memory>

namespace {

// 系统默认走的那块网卡的 IPv4 地址（SDK 列出的网卡，默认的在最前面）：网线、WiFi 都连着时附近的设备连它
// （LanDiscovery）。没有时为空
QString defaultAddress()
{
    std::array<rcs_network_info, 16> networks{};
    uint32_t count = 0;
    if (rcs_list_networks(networks.data(), sizeof(rcs_network_info), uint32_t(networks.size()), &count) != RCS_OK)
        return {};
    for (uint32_t i = 0; i < std::min<uint32_t>(count, uint32_t(networks.size())); ++i) {
        if (!(networks[i].flags & RCS_NETWORK_FLAG_DEFAULT) || !networks[i].ipv4)
            continue;
        const QNetworkInterface interface = QNetworkInterface::interfaceFromIndex(int(networks[i].index));
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return entry.ip().toString();
        }
    }
    return {};
}

} // namespace

// 后台服务怎么运行（rc/programs.h、rc/system_service.h、docs/install-and-first-run.md）：
// - 开发版（不带参数）：以当前用户运行，界面程序启动它，只有这个用户的界面能连它
// - 装好的 Windows：系统服务（--system-service）里的看护者在控制台会话里以 SYSTEM 启动它（--helper），
//   整台电脑一份身份；跟着锁屏、登录界面、UAC；注销、换用户时被重启，连着的人自己重连
// - 装好的 macOS：LaunchDaemon 以 root 启动它（--daemon），整台电脑一份身份；录屏、声音、键鼠在各个会话的助手里
// 装好时每个用户的界面程序都连它，只认控制台前的用户和管理员（client_rights.h）
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Bilin"));
    QCoreApplication::setApplicationName(QStringLiteral("BilinService"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const QStringList arguments = QCoreApplication::arguments();
    // SDK 的日志也进本程序的日志（service.log；系统服务的看护者进 system-service.log）
    rcsdk::setLogCallback(RCS_LOG_INFO, [](const rcs_log_record &record) {
        if (record.level <= RCS_LOG_WARNING)
            qWarning("[sdk %s] %s", record.module, record.message);
        else
            qInfo("[sdk %s] %s", record.module, record.message);
    });
#ifdef Q_OS_WIN
    if (arguments.contains(rc::program::SystemServiceArgument))
        return system_mode::runSystemService();
    const bool installed = arguments.contains(rc::program::HelperArgument);
#else
    const bool installed = arguments.contains(rc::program::DaemonArgument);
#endif

    QString directory = rc::stateDirectory();
    if (installed) {
        if (const QString problem = system_mode::prepareStateDirectory(); !problem.isEmpty()) {
            qCritical("数据目录不对：%s", qUtf8Printable(problem));
            return 1;
        }
        directory = rc::system_service::stateDirectory();
    }
#ifdef Q_OS_WIN
    const ShareCapture capture{installed ? ShareCapture::Where::SecureDesktops : ShareCapture::Where::Here, {}};
#else
    const ShareCapture capture{installed ? ShareCapture::Where::Agents : ShareCapture::Where::Here,
                               installed ? system_mode::agentKey() : QByteArray()};
    if (installed && capture.agentKey.size() != 32)
        return 1;
#endif
    // 一份身份只运行一个后台服务。锁在进程结束时由系统放开，异常退出留下的锁下次能接过来
    QLockFile lock(directory + QStringLiteral("/service.lock"));
    if (!lock.tryLock(0))
        return 0;
    rc::openLogFile(directory, QStringLiteral("service.log"));
    if (installed) {
        if (const QString problem = system_mode::programProblem(); !problem.isEmpty()) {
            qCritical("程序没保护好，不以 SYSTEM / root 运行：%s", qUtf8Printable(problem));
            return 1;
        }
    }

    rc::protocol::ServiceInfo info;
    info.program = QCoreApplication::applicationFilePath();
    info.modified = QFileInfo(info.program).lastModified().toMSecsSinceEpoch();
    qInfo("后台服务启动：%s", qUtf8Printable(info.program));
    // SDK 的模块在任何会话之前注册：剪贴板、互传文件、保持唤醒（macOS 装好时机器那一半在助手里，助手也注册）
    rcsdk::registerModule(*rcs_clipboard_module());
    rcsdk::registerModule(*rcs_file_transfer_module());
    rcsdk::registerModule(*rcs_power_module());
    rcsdk::registerModule(*rcs_thumbnail_module());

    // 设备密钥由 SDK 生成（P-256）
    DeviceStore store(directory, [] {
        const rcsdk::DeviceKey key = rcsdk::generateDeviceKey();
        return QByteArray(reinterpret_cast<const char *>(key.data()), qsizetype(key.size()));
    });
    // 服务器的地址在编译时定（CMake 的 RC_SERVER_URL）
    ServerLink serverLink(store, QUrl(QStringLiteral(RC_SERVER_URL)), {});
    ControlServer server(store, info,
                         installed ? ControlServer::Clients::ConsoleUsers : ControlServer::Clients::CurrentUser);
    // 装好时由系统管着：不跟着界面退出
    QObject::connect(&server, &ControlServer::quitRequested, &app, [installed] {
        if (installed) {
            qInfo("界面让后台服务退出：它是系统服务，不退出");
            return;
        }
        qInfo("界面让后台服务退出");
        QCoreApplication::quit();
    });

    // 别人连本机：界面上显示请求连接的人、连着的人，由界面上的人决定；没有界面连着时没人能决定。
    // 我的设备（账号下别的设备）不要密码、不用点允许（Sharing）
    // 和连着本机的人没传完的文件（下次接着传）
    rc::InterruptedTransfers transfers(directory + QStringLiteral("/share-transfers.json"));
    Sharing host(store, transfers, capture);
    host.setTrustedKeys(serverLink.trustedKeys());
    QObject::connect(&serverLink, &ServerLink::accountChanged, &host, [&] { host.setTrustedKeys(serverLink.trustedKeys()); });
    server.publish(rc::protocol::ShareTopic, host.state().toJson());
    QObject::connect(&host, &Sharing::stateChanged, &server,
                     [&] { server.publish(rc::protocol::ShareTopic, host.state().toJson()); });
    QObject::connect(&server, &ControlServer::clientsChanged, &host,
                     [&] { host.setAttended(server.clientCount() > 0); });
    QObject::connect(&server, &ControlServer::admitRequested, &host, &Sharing::admit);
    QObject::connect(&server, &ControlServer::refuseRequested, &host, &Sharing::refuse);
    QObject::connect(&server, &ControlServer::disconnectRequested, &host, &Sharing::disconnectViewer);
    QObject::connect(&server, &ControlServer::controlRequested, &host, &Sharing::setControl);
    QObject::connect(&server, &ControlServer::clipboardRequested, &host, &Sharing::setClipboard);
    // 本机桌面的背景：有人连进来时问界面程序要（后台服务读不到登录的人的背景），发给连着的人
    QObject::connect(&server, &ControlServer::wallpaperReceived, &host, &Sharing::setWallpaper);
    QObject::connect(&host, &Sharing::wallpaperWanted, &server,
                     [&] { server.sendEvent(rc::protocol::WallpaperWanted, {}); });
    QObject::connect(&server, &ControlServer::permissionRequested, &host, &Sharing::requestPermission);
    QObject::connect(&server, &ControlServer::restartShareRequested, &host, &Sharing::restart);
    // 网页版：浏览器经服务器上的转发进程来连本机
    QObject::connect(&serverLink, &ServerLink::webViewerRequested, &host,
                     [&host, relay = RelayChannel::urlFor(QUrl(QStringLiteral(RC_SERVER_URL)))](
                         const QString &ticket, const QByteArray &key) { host.webViewer(relay, ticket, key); });
    QObject::connect(&host, &Sharing::webAnswered, &serverLink, &ServerLink::sendWebReady);
    // 有人连着本机时界面程序要在（本机的人看得到谁在连）：意外没了就拉起来，界面要退出就先断开连着的人
    AppKeeper keeper(installed ? system_mode::launchApp : launchAppBeside, [&host] { host.disconnectAll(); });
    QObject::connect(&server, &ControlServer::clientsChanged, &keeper,
                     [&] { keeper.setAttended(server.clientCount() > 0); });
    QObject::connect(&host, &Sharing::stateChanged, &keeper,
                     [&] { keeper.setWatched(!host.state().viewers.isEmpty()); });
    QObject::connect(&server, &ControlServer::appQuittingRequested, &keeper, &AppKeeper::appQuitting);

    // 对话：连着本机的人说的话交给界面，界面上的人说的发给对方；没有界面连着时告诉对方没人看
    ShareChat chat([&host](int id, const QByteArray &data) { return host.sendData(id, rc::peer::ChatChannel, data); });
    host.addChannel(rc::peer::ChatChannel, [&chat](int id, const QByteArray &data) { chat.receive(id, data); });
    QObject::connect(&host, &Sharing::stateChanged, &chat, [&] { chat.setViewers(host.state().viewers); });
    QObject::connect(&server, &ControlServer::clientsChanged, &chat,
                     [&] { chat.setAttended(server.clientCount() > 0); });
    QObject::connect(&server, &ControlServer::chatRequested, &chat, &ShareChat::send);
    QObject::connect(&chat, &ShareChat::received, &server, [&server](int id, const QString &text) {
        server.sendEvent(rc::protocol::ChatReceived, {{QStringLiteral("id"), id}, {QStringLiteral("text"), text}});
    });
    QObject::connect(&chat, &ShareChat::failed, &server, [&server](int id, int ref, const QString &reason) {
        server.sendEvent(rc::protocol::ChatFailed, {{QStringLiteral("id"), id},
                                                    {QStringLiteral("ref"), ref},
                                                    {QStringLiteral("reason"), reason}});
    });

    // P2P 和中继用的服务器（服务器发来的）：分享会话用，界面里去连别人的会话也用（ice 主题，publishNetwork）
    QObject::connect(&serverLink, &ServerLink::iceChanged, &host, [&] { host.setIceServers(serverLink.ice()); });

    // 和连着本机的人互传文件：对方发来的由界面上的人决定收不收、存到哪里
    server.publish(rc::protocol::FilesTopic, rc::transfersToJson(host.transfers()));
    QObject::connect(&host, &Sharing::transfersChanged, &server,
                     [&] { server.publish(rc::protocol::FilesTopic, rc::transfersToJson(host.transfers())); });
    QObject::connect(&server, &ControlServer::sendFilesRequested, &host, &Sharing::sendFiles);
    QObject::connect(&server, &ControlServer::acceptTransferRequested, &host, &Sharing::acceptTransfer);
    QObject::connect(&server, &ControlServer::rejectTransferRequested, &host, &Sharing::rejectTransfer);
    QObject::connect(&server, &ControlServer::cancelTransferRequested, &host, &Sharing::cancelTransfer);
    QObject::connect(&server, &ControlServer::retryTransferRequested, &host, &Sharing::retryTransfer);
    QObject::connect(&server, &ControlServer::removeTransferRequested, &host, &Sharing::removeTransfer);

    // 本机的连接记录（界面上的通知）：谁什么时候连过本机、请求没成的、输错密码的、互传的文件
    ConnectionHistory history(directory + QStringLiteral("/history.json"));
    server.publish(rc::protocol::HistoryTopic, history.state().toJson());
    QObject::connect(&history, &ConnectionHistory::changed, &server,
                     [&] { server.publish(rc::protocol::HistoryTopic, history.state().toJson()); });
    QObject::connect(&host, &Sharing::viewerConnected, &history, &ConnectionHistory::connected);
    QObject::connect(&host, &Sharing::viewerGone, &history, &ConnectionHistory::gone);
    QObject::connect(&host, &Sharing::requestFailed, &history, &ConnectionHistory::refused);
    QObject::connect(&host, &Sharing::wrongPin, &history, &ConnectionHistory::wrongPin);
    QObject::connect(&host, &Sharing::powerDone, &history, &ConnectionHistory::power);
    QObject::connect(&host, &Sharing::transferCompleted, &history, [&](const rc::Transfer &transfer) {
        const auto viewers = host.state().viewers;
        const auto peer = std::find_if(viewers.cbegin(), viewers.cend(),
                                       [&transfer](const rc::protocol::SharePeer &v) { return v.id == transfer.viewer; });
        // 传完的那一刻对方刚好走了：不知道是谁，不记
        if (peer != viewers.cend())
            history.transferred(*peer, transfer);
    });
    QObject::connect(&server, &ControlServer::clearHistoryRequested, &history, &ConnectionHistory::clear);

    // 远程唤醒（docs/devices-and-accounts.md）：本机能唤醒它的网卡告诉服务器（每次连上都重新列：网络可能换了）；
    // 账号下的设备睡着了、别处要叫醒它时，和它在同一个网段就帮忙发唤醒包，连接记录里记一笔
    WakeHelper wakeHelper;
    QObject::connect(&serverLink, &ServerLink::stateChanged, &wakeHelper, [&] {
        if (serverLink.state() == ServerLink::State::Online)
            wakeHelper.listInterfaces();
    });
    QObject::connect(&wakeHelper, &WakeHelper::interfacesListed, &serverLink, &ServerLink::setWakeInterfaces);
    QObject::connect(&serverLink, &ServerLink::wakeRequested, &wakeHelper,
                     [&](const QString &request, const QString &deviceId, const QString &name, const QString &by,
                         const QList<rc::server::WakeInterface> &interfaces) {
                         const QLatin1StringView result = wakeHelper.wake(interfaces);
                         serverLink.sendWakeAnswer(request, result);
                         if (result == rc::server::WakeSent)
                             history.woke(deviceId, name, by);
                     });

    // 账号：经服务器，结果给界面
    service_wiring::connectAccount(server, serverLink);

    // 反馈：带上本程序的日志（这次和上次运行的）；装好时还有 Windows 系统服务的看护者、macOS 登录界面上的助手的
    QStringList logFiles{directory + QStringLiteral("/service.log"), directory + QStringLiteral("/service.log.previous")};
    if (installed) {
#ifdef Q_OS_WIN
        const QString watcher = directory + QStringLiteral("/system-service.log");
#else
        const QString watcher = directory + QStringLiteral("/agent-loginwindow.log");
#endif
        logFiles << watcher << watcher + QStringLiteral(".previous");
    }
    FeedbackSender feedback(store, HttpUpdateSource::baseFor(QUrl(QStringLiteral(RC_SERVER_URL))), logFiles);
    service_wiring::connectFeedback(server, feedback);

    // 局域网：附近的设备（发现），不经服务器的信令
    LanDiscovery discovery(store, {.defaultAddress = defaultAddress});
    LanSignaling lan(store, {});
    service_wiring::publishNetwork(server, serverLink, discovery);

    // 信令：界面去连别人的会话 ↔ 服务器或局域网 ↔ 本机的分享会话（谁的交给谁、走哪条路见 Signaling）
    Signaling signaling([&discovery](const QString &deviceId) { return discovery.addressOf(deviceId); });
    service_wiring::connectViewerSignaling(server, serverLink, lan, signaling);
    QObject::connect(&host, &Sharing::answer, &signaling, &Signaling::fromSharer);
    QObject::connect(&host, &Sharing::declined, &signaling, &Signaling::declineFromSharer);
    QObject::connect(&host, &Sharing::viewerConnected, &signaling,
                     [&](const rc::protocol::SharePeer &, const QString &session) { signaling.keepSharer(session); });
    QObject::connect(&host, &Sharing::viewerGone, &signaling,
                     [&](const QString &, const QString &session) { signaling.releaseSharer(session); });
    QObject::connect(&signaling, &Signaling::toSharer, &host, &Sharing::receiveSignal);
    if (!server.listen(installed ? rc::system_service::endpoint() : rc::ipc::serviceEndpoint()))
        return 1;
    // 装好时系统要它结束（注销、换用户、服务停了）：连着的人等它回来
    if (installed) {
        system_mode::onStopRequest([&host] {
            host.stopForRestart();
            QCoreApplication::quit();
        });
    }
    // 装好的程序的新版本（docs/admin.md）：有了告诉界面，本机的人点了"更新"再下载、安装
    std::unique_ptr<HttpUpdateSource> updateSource;
    std::unique_ptr<Updater> updater;
    if (installed && !rc::release::platform().isEmpty()) {
        updateSource = std::make_unique<HttpUpdateSource>(HttpUpdateSource::baseFor(QUrl(QStringLiteral(RC_SERVER_URL))));
        updater = std::make_unique<Updater>(
            Updater::Options{rc::system_service::updateInstaller(), rc::release::platform(),
                             QVersionNumber::fromString(QCoreApplication::applicationVersion()), rc::release::publicKey()},
            *updateSource, system_mode::installUpdate);
        server.publish(rc::protocol::UpdateTopic, updater->state().toJson());
        QObject::connect(updater.get(), &Updater::stateChanged, &server,
                         [&] { server.publish(rc::protocol::UpdateTopic, updater->state().toJson()); });
        QObject::connect(&server, &ControlServer::installUpdateRequested, updater.get(), &Updater::installNow);
        QObject::connect(&server, &ControlServer::checkUpdateRequested, updater.get(), &Updater::checkNow);
        // 装完了：通知里记一笔，界面程序重新打开（是本机的人在界面上点的更新）
        if (updater->justUpdated()) {
            history.updated(QCoreApplication::applicationVersion());
            system_mode::launchApp();
        }
        updater->start();
    }
    lan.listen();
    serverLink.start();
    return QCoreApplication::exec();
}
