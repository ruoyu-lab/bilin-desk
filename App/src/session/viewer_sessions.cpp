#include "viewer_sessions.h"

#include "account.h"
#include "conversations.h"
#include "service_client.h"

#include <rc/protocol.h>

#include <QJSEngine>

namespace protocol = rc::protocol;

namespace {

ViewerSessions *instance = nullptr;

} // namespace

ViewerSessions::ViewerSessions(ServiceClient &service, Account &account, void *decodeDevice,
                               Conversations &conversations, rc::InterruptedTransfers &transfers, QObject *parent)
    : QObject(parent)
    , service_(service)
    , account_(account)
    , decodeDevice_(decodeDevice)
    , conversations_(conversations)
    , transfers_(transfers)
{
    connect(&service_, &ServiceClient::eventReceived, this, &ViewerSessions::serviceEvent);
}

void ViewerSessions::setInstance(ViewerSessions *sessions)
{
    instance = sessions;
}

ViewerSessions *ViewerSessions::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

ViewerSession *ViewerSessions::open(const QString &key, const QString &target)
{
    if (ViewerSession *existing = sessions_.value(key))
        return existing;
    auto *session = new ViewerSession(target, service_, decodeDevice_, *conversations_.get(key), transfers_,
                                      [this](const QString &deviceId) { return account_.publicKey(deviceId); }, this);
    sessions_.insert(key, session);
    return session;
}

ViewerSession *ViewerSessions::get(const QString &key) const
{
    return sessions_.value(key);
}

void ViewerSessions::close(const QString &key)
{
    ViewerSession *session = sessions_.take(key);
    if (!session)
        return;
    session->stop();
    session->deleteLater();
}

void ViewerSessions::serviceEvent(const QString &name, const QJsonObject &arguments)
{
    ViewerSession *session = byConnection(arguments.value(QLatin1StringView("session")).toString());
    if (!session)
        return;
    if (name == protocol::Signal)
        session->receiveSignal(arguments.value(QLatin1StringView("text")).toString());
    else if (name == protocol::Declined)
        session->declined(arguments.value(QLatin1StringView("reason")).toString());
    else if (name == protocol::Undeliverable)
        session->undeliverable(arguments.value(QLatin1StringView("reason")).toString());
}

ViewerSession *ViewerSessions::byConnection(const QString &connection) const
{
    if (connection.isEmpty())
        return nullptr;
    for (ViewerSession *session : sessions_) {
        if (session->connection() == connection)
            return session;
    }
    return nullptr;
}
