#include "local_service.h"

#include "service_wiring.h"

#include <rc/peer_protocol.h>
#include <rc/protocol.h>

#include <rcsdk/rcsdk.hpp>

#include <QCoreApplication>
#include <QFileInfo>

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
    , discovery_(store_, {})
    , lan_(store_, {})
    , signaling_([this](const QString &deviceId) { return discovery_.addressOf(deviceId); })
{
    service_wiring::connectAccount(server_, link_);
    service_wiring::publishNetwork(server_, link_, discovery_);
    service_wiring::connectViewerSignaling(server_, link_, lan_, signaling_);
    connect(&signaling_, &Signaling::toSharer, this,
            [this](const QString &from, const QString &, const QString &session) {
                signaling_.declineFromSharer(from, session, QLatin1StringView(rc::peer::NotAccepting));
            });
}

bool LocalService::start(const QString &endpoint)
{
    if (!server_.listen(endpoint))
        return false;
    lan_.listen();
    link_.start();
    return true;
}
