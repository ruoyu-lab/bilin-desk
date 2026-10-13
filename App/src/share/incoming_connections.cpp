#include "incoming_connections.h"

#include "service_client.h"

#include <rc/device_id.h>

#include <QDesktopServices>
#include <QJSEngine>
#include <QUrl>

namespace protocol = rc::protocol;

namespace {

IncomingConnections *instance = nullptr;

QVariantList toVariant(const QList<protocol::SharePeer> &peers)
{
    QVariantList list;
    for (const protocol::SharePeer &peer : peers) {
        list.append(QVariantMap{
            {QStringLiteral("id"), peer.id},
            {QStringLiteral("deviceId"), rc::DeviceId::parse(peer.deviceId).display()},
            {QStringLiteral("name"), peer.name},
            {QStringLiteral("platform"), peer.platform},
            {QStringLiteral("control"), peer.control},
        });
    }
    return list;
}

} // namespace

IncomingConnections::IncomingConnections(ServiceClient &service, Conversations &conversations, QObject *parent)
    : QObject(parent)
    , service_(service)
    , chat_(service, conversations)
    , files_(new IncomingFiles(service, this))
    , history_(new IncomingHistory(service, this))
{
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic != protocol::ShareTopic)
            return;
        const auto state = protocol::ShareState::fromJson(service_.topic(protocol::ShareTopic));
        if (state == state_)
            return;
        state_ = state;
        chat_.setViewers(state_.viewers);
        emit changed();
    });
}

void IncomingConnections::setInstance(IncomingConnections *connections)
{
    instance = connections;
}

IncomingConnections *IncomingConnections::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

QVariantList IncomingConnections::requests() const
{
    return toVariant(state_.requests);
}

QVariantList IncomingConnections::viewers() const
{
    return toVariant(state_.viewers);
}

void IncomingConnections::admit(int id, bool control)
{
    service_.send(protocol::Admit, {{QStringLiteral("id"), id}, {QStringLiteral("control"), control}});
}

void IncomingConnections::refuse(int id)
{
    service_.send(protocol::Refuse, {{QStringLiteral("id"), id}});
}

void IncomingConnections::disconnectViewer(int id)
{
    service_.send(protocol::Disconnect, {{QStringLiteral("id"), id}});
}

void IncomingConnections::setControl(int id, bool control)
{
    service_.send(protocol::SetControl, {{QStringLiteral("id"), id}, {QStringLiteral("value"), control}});
}

void IncomingConnections::setClipboard(bool clipboard)
{
    service_.send(protocol::SetClipboard, {{QStringLiteral("value"), clipboard}});
}

// 只在 macOS 上会缺授权
void IncomingConnections::requestPermission(const QString &permission)
{
    service_.send(protocol::RequestPermission, {{QStringLiteral("permission"), permission}});
    const QString pane = permission == protocol::ScreenPermission ? QStringLiteral("Privacy_ScreenCapture")
                                                                  : QStringLiteral("Privacy_Accessibility");
    QDesktopServices::openUrl(QUrl(QStringLiteral("x-apple.systempreferences:com.apple.preference.security?") + pane));
}

void IncomingConnections::retry()
{
    service_.send(protocol::RestartShare);
}
