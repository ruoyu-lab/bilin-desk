#include "this_device.h"

#include "service_client.h"

#include <rc/device_id.h>

#include <QJSEngine>

namespace protocol = rc::protocol;

namespace {

ThisDevice *instance = nullptr;

} // namespace

ThisDevice::ThisDevice(ServiceClient &service, QObject *parent)
    : QObject(parent)
    , service_(service)
{
    connect(&service_, &ServiceClient::stateChanged, this, &ThisDevice::serviceStateChanged);
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic == protocol::UpdateTopic) {
            const auto update = protocol::UpdateState::fromJson(service_.topic(protocol::UpdateTopic));
            if (update != update_ || !updatable_) {
                update_ = update;
                updatable_ = true;
                emit updateChanged();
            }
            return;
        }
        if (topic != protocol::DeviceTopic)
            return;
        const auto state = protocol::DeviceState::fromJson(service_.topic(protocol::DeviceTopic));
        if (state == state_)
            return;
        state_ = state;
        emit changed();
    });
}

void ThisDevice::setInstance(ThisDevice *device)
{
    instance = device;
}

ThisDevice *ThisDevice::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

QString ThisDevice::serviceState() const
{
    switch (service_.state()) {
    case ServiceClient::State::Connecting:
        return QStringLiteral("connecting");
    case ServiceClient::State::Ready:
        return QStringLiteral("ready");
    case ServiceClient::State::Unavailable:
        return QStringLiteral("unavailable");
    }
    Q_UNREACHABLE_RETURN({});
}

QString ThisDevice::deviceId() const
{
    return rc::DeviceId::parse(state_.deviceId).display();
}

void ThisDevice::setDeviceName(const QString &name)
{
    service_.send(protocol::SetDeviceName, {{QStringLiteral("name"), name}});
}

void ThisDevice::setAcceptIncoming(bool accept)
{
    service_.send(protocol::SetAcceptIncoming, {{QStringLiteral("value"), accept}});
}

void ThisDevice::setLanDiscovery(bool enabled)
{
    service_.send(protocol::SetLanDiscovery, {{QStringLiteral("value"), enabled}});
}

void ThisDevice::setUnattended(bool unattended)
{
    service_.send(protocol::SetUnattended, {{QStringLiteral("value"), unattended}});
}

void ThisDevice::checkUpdate()
{
    service_.send(protocol::CheckUpdate);
}

bool ThisDevice::systemService() const
{
    return service_.systemService();
}

void ThisDevice::installUpdate()
{
    service_.send(protocol::InstallUpdate);
}

void ThisDevice::regeneratePassword()
{
    service_.send(protocol::RegeneratePassword);
}

void ThisDevice::retry()
{
    service_.retry();
}

void ThisDevice::quitService()
{
    service_.stopService();
}
