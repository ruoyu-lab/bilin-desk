#include "service_wiring.h"

#include "control_server.h"
#include "feedback_sender.h"
#include "lan_discovery.h"
#include "lan_signaling.h"
#include "server_link.h"
#include "signaling.h"

#include <rc/protocol.h>
#include <rc/server_protocol.h>

namespace protocol = rc::protocol;

namespace service_wiring {

void connectAccount(ControlServer &server, ServerLink &link)
{
    server.publish(protocol::AccountTopic, link.account().toJson());
    QObject::connect(&link, &ServerLink::accountChanged, &server,
                     [&] { server.publish(protocol::AccountTopic, link.account().toJson()); });
    const auto accountFailed = [&server](const QString &reason) {
        server.sendEvent(protocol::AccountFailed, {{QStringLiteral("reason"), reason}});
    };
    QObject::connect(&link, &ServerLink::accountFailed, &server, accountFailed);
    const auto noServer = [accountFailed](bool sent) {
        if (!sent)
            accountFailed(protocol::NoServer);
    };
    QObject::connect(&server, &ControlServer::sendCodeRequested, &link,
                     [&link, noServer](const QString &email, const QString &purpose) {
                         noServer(link.sendCode(email, purpose));
                     });
    QObject::connect(&link, &ServerLink::codeSent, &server, [&server](const QString &purpose) {
        server.sendEvent(protocol::CodeSent, {{QStringLiteral("purpose"), purpose}});
    });
    QObject::connect(&server, &ControlServer::registerRequested, &link,
                     [&link, noServer](const QString &email, const QString &password, const QString &nickname,
                                       const QString &code) {
                         noServer(link.registerAccount(email, password, nickname, code));
                     });
    QObject::connect(&server, &ControlServer::signInRequested, &link,
                     [&link, noServer](const QString &email, const QString &password) {
                         noServer(link.signIn(email, password));
                     });
    QObject::connect(&server, &ControlServer::resetPasswordRequested, &link,
                     [&link, noServer](const QString &email, const QString &code, const QString &password) {
                         noServer(link.resetPassword(email, code, password));
                     });
    QObject::connect(&server, &ControlServer::signOutRequested, &link, [&link, noServer] { noServer(link.signOut()); });
    QObject::connect(&server, &ControlServer::removeDeviceRequested, &link,
                     [&link, noServer](const QString &deviceId) { noServer(link.removeDevice(deviceId)); });
    QObject::connect(&server, &ControlServer::nicknameRequested, &link,
                     [&link, noServer](const QString &nickname) { noServer(link.setNickname(nickname)); });
    QObject::connect(&server, &ControlServer::changePasswordRequested, &link,
                     [&link, noServer](const QString &oldPassword, const QString &newPassword) {
                         noServer(link.changePassword(oldPassword, newPassword));
                     });
    QObject::connect(&server, &ControlServer::deleteAccountRequested, &link,
                     [&link, noServer](const QString &password) { noServer(link.deleteAccount(password)); });
    QObject::connect(&link, &ServerLink::passwordChanged, &server,
                     [&server] { server.sendEvent(protocol::PasswordChanged, {}); });
}

void publishNetwork(ControlServer &server, ServerLink &link, LanDiscovery &discovery)
{
    server.publish(protocol::IceTopic, link.ice().toJson());
    QObject::connect(&link, &ServerLink::iceChanged, &server,
                     [&] { server.publish(protocol::IceTopic, link.ice().toJson()); });
    server.publish(protocol::NearbyTopic, discovery.state().toJson());
    QObject::connect(&discovery, &LanDiscovery::changed, &server,
                     [&] { server.publish(protocol::NearbyTopic, discovery.state().toJson()); });
}

void connectViewerSignaling(ControlServer &server, ServerLink &link, LanSignaling &lan, Signaling &signaling)
{
    QObject::connect(&server, &ControlServer::signalRequested, &signaling, &Signaling::fromViewer);
    QObject::connect(&server, &ControlServer::signalEnded, &signaling, &Signaling::endViewer);
    QObject::connect(&link, &ServerLink::signalReceived, &signaling, &Signaling::fromServer);
    QObject::connect(&link, &ServerLink::declined, &signaling, &Signaling::declinedByPeer);
    QObject::connect(&link, &ServerLink::undeliverable, &signaling,
                     [&signaling](const QString &to, const QString &session, const QString &reason) {
                         signaling.undeliverable(to, session,
                                                 reason == rc::server::TooMany ? protocol::TooMany : protocol::Offline);
                     });
    QObject::connect(&lan, &LanSignaling::received, &signaling, &Signaling::fromLan);
    QObject::connect(&lan, &LanSignaling::declined, &signaling, &Signaling::declinedByPeer);
    QObject::connect(&lan, &LanSignaling::unreachable, &signaling, &Signaling::lanUnreachable);
    QObject::connect(&signaling, &Signaling::forgotten, &lan, &LanSignaling::close);
    QObject::connect(&signaling, &Signaling::toServer, &link,
                     [&link, &signaling](const QString &to, const QString &session, const QString &text,
                                         const rc::ConnectionMode &mode) {
                         if (!link.sendSignal(to, session, text, mode))
                             signaling.undeliverable(to, session, protocol::NoServer);
                     });
    QObject::connect(&signaling, &Signaling::declineToServer, &link, &ServerLink::sendDecline);
    QObject::connect(&signaling, &Signaling::toLan, &lan, &LanSignaling::send);
    QObject::connect(&signaling, &Signaling::answerToLan, &lan, &LanSignaling::answer);
    QObject::connect(&signaling, &Signaling::declineToLan, &lan, &LanSignaling::decline);
    QObject::connect(&signaling, &Signaling::toViewer, &server,
                     [&server](const QString &from, const QString &session, const QString &text) {
                         server.sendEvent(protocol::Signal, {{QStringLiteral("from"), from},
                                                             {QStringLiteral("session"), session},
                                                             {QStringLiteral("text"), text}});
                     });
    QObject::connect(&signaling, &Signaling::viewerDeclined, &server,
                     [&server](const QString &from, const QString &session, const QString &reason) {
                         server.sendEvent(protocol::Declined, {{QStringLiteral("from"), from},
                                                               {QStringLiteral("session"), session},
                                                               {QStringLiteral("reason"), reason}});
                     });
    QObject::connect(&signaling, &Signaling::viewerUndeliverable, &server,
                     [&server](const QString &to, const QString &session, const QString &reason) {
                         server.sendEvent(protocol::Undeliverable, {{QStringLiteral("to"), to},
                                                                    {QStringLiteral("session"), session},
                                                                    {QStringLiteral("reason"), reason}});
                     });
}

void connectFeedback(ControlServer &server, FeedbackSender &feedback)
{
    QObject::connect(&server, &ControlServer::feedbackLogsRequested, &feedback, [&server, &feedback](qintptr socket) {
        server.sendEventTo(socket, protocol::FeedbackLogs,
                           {{QStringLiteral("logs"), rc::feedback::packLogs(feedback.logs())}});
    });
    QObject::connect(&server, &ControlServer::feedbackRequested, &feedback,
                     [&server, &feedback](qintptr socket, const QString &text, const QString &contact,
                                          const QJsonArray &packed) {
                         const auto done = [&server, socket](const QString &reason) {
                             if (reason.isEmpty())
                                 server.sendEventTo(socket, protocol::FeedbackSent, {});
                             else
                                 server.sendEventTo(socket, protocol::FeedbackFailed, {{QStringLiteral("reason"), reason}});
                         };
                         const std::optional<QList<rc::feedback::Log>> logs = rc::feedback::unpackLogs(packed);
                         if (!logs) {
                             qWarning("界面发来的反馈日志不对");
                             done(protocol::Failed);
                             return;
                         }
                         feedback.send(text, contact, *logs, done);
                     });
}

} // namespace service_wiring
