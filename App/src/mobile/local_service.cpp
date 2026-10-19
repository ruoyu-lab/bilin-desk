#include "local_service.h"

#include "service_wiring.h"

#include <rc/peer_protocol.h>
#include <rc/protocol.h>
#include <rc/release.h>

#include <rcsdk/rcsdk.hpp>

#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>
#include <QVersionNumber>

namespace {

rc::protocol::ServiceInfo serviceInfo()
{
    rc::protocol::ServiceInfo info;
    info.program = QCoreApplication::applicationFilePath();
    info.modified = QFileInfo(info.program).lastModified().toMSecsSinceEpoch();
    return info;
}

} // namespace

LocalService::LocalService(const QString &directory, QObject *parent)
    : QObject(parent)
    // 设备密钥由 SDK 生成（P-256）
    , store_(directory, [] {
        const rcsdk::DeviceKey key = rcsdk::generateDeviceKey();
        return QByteArray(reinterpret_cast<const char *>(key.data()), qsizetype(key.size()));
    })
    // 服务器的地址在编译时定（CMake 的 RC_SERVER_URL）
    , link_(store_, QUrl(QStringLiteral(RC_SERVER_URL)), {})
    , server_(store_, serviceInfo())
    , discovery_(store_, {.announce = false})
    , lan_(store_, {})
    , signaling_([this](const QString &deviceId) { return discovery_.addressOf(deviceId); })
    // 反馈：后台服务在本进程里，它的日志也在界面的 app.log 里（界面那边带），这里没有自己的
    , feedback_(store_, HttpUpdateSource::baseFor(QUrl(QStringLiteral(RC_SERVER_URL))), {})
{
    service_wiring::connectAccount(server_, link_);
    service_wiring::publishNetwork(server_, link_, discovery_);
    service_wiring::connectViewerSignaling(server_, link_, lan_, signaling_);
    service_wiring::connectFeedback(server_, feedback_);
    connect(&signaling_, &Signaling::toSharer, this,
            [this](const QString &from, const QString &, const QString &session) {
                signaling_.declineFromSharer(from, session, QLatin1StringView(rc::peer::NotAccepting));
            });
#ifdef Q_OS_ANDROID
    // 新版本：安装包下载到程序自己的目录，经 Qt 的 FileProvider（AndroidManifest.xml 的 qtprovider）交给系统的
    // 安装界面。手机上程序开着的时间短，一开就问
    const QString installer = directory + QStringLiteral("/updates/Bilin-update.apk");
    updateSource_ = std::make_unique<HttpUpdateSource>(HttpUpdateSource::baseFor(QUrl(QStringLiteral(RC_SERVER_URL))));
    updater_ = std::make_unique<Updater>(
        Updater::Options{.installer = installer,
                         .platform = rc::release::platform(),
                         .version = QVersionNumber::fromString(QCoreApplication::applicationVersion()),
                         .publicKey = rc::release::publicKey(),
                         .firstCheckMs = 10 * 1000},
        *updateSource_, [installer] { return QDesktopServices::openUrl(QUrl::fromLocalFile(installer)); });
    server_.publish(rc::protocol::UpdateTopic, updater_->state().toJson());
    connect(updater_.get(), &Updater::stateChanged, this,
            [this] { server_.publish(rc::protocol::UpdateTopic, updater_->state().toJson()); });
    connect(&server_, &ControlServer::installUpdateRequested, updater_.get(), &Updater::installNow);
    connect(&server_, &ControlServer::checkUpdateRequested, updater_.get(), &Updater::checkNow);
    // 装上了系统会结束本程序；回到程序时还在"安装"，就是在安装界面取消了
    connect(qApp, &QGuiApplication::applicationStateChanged, updater_.get(), [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            updater_->installAbandoned();
    });
#endif
}

bool LocalService::start(const QString &endpoint)
{
    if (!server_.listen(endpoint))
        return false;
    lan_.listen();
    link_.start();
    if (updater_)
        updater_->start();
    return true;
}
