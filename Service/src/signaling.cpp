#include "signaling.h"

#include <rc/protocol.h>

Signaling::Signaling(std::function<QString(const QString &deviceId)> nearbyAddress, QObject *parent)
    : QObject(parent)
    , nearbyAddress_(std::move(nearbyAddress))
{
    forgetTimer_.setInterval(30 * 1000);
    connect(&forgetTimer_, &QTimer::timeout, this, &Signaling::forget);
    forgetTimer_.start();
}

void Signaling::fromViewer(const QString &to, const QString &address, const QString &session, const QString &text,
                           const rc::ConnectionMode &mode)
{
    const QString lanAddress = address.isEmpty() ? nearbyAddress_(to) : address;
    Outgoing &outgoing = viewerSessions_[session];
    // 按 IP 地址连的：对面是哪台设备，第一个回答来了才知道（fromLan），之后的信令里还是空的
    if (!to.isEmpty())
        outgoing.to = to;
    outgoing.address = lanAddress;
    if (lanAddress.isEmpty())
        emit toServer(to, session, text, mode);
    else
        emit toLan(lanAddress, session, text, mode);
}

void Signaling::fromServer(const QString &from, const QString &name, const QString &session, const QString &text,
                           const rc::ConnectionMode &mode)
{
    const auto viewer = viewerSessions_.constFind(session);
    if (viewer == viewerSessions_.cend()) {
        incoming(session, false, mode);
        emit toSharer(from, name, session, text, mode);
        return;
    }
    // 回答只能来自当初发给的那台设备
    if (!viewer->address.isEmpty() || viewer->to != from) {
        qWarning("设备 %s 冒用了别的连接的编号，不理", qUtf8Printable(from));
        return;
    }
    emit toViewer(from, session, text);
}

void Signaling::fromLan(const QString &address, const QString &from, const QString &name, const QString &session,
                        const QString &text, const rc::ConnectionMode &mode)
{
    const auto viewer = viewerSessions_.find(session);
    if (viewer == viewerSessions_.end()) {
        incoming(session, true, mode);
        emit toSharer(from, name, session, text, mode);
        return;
    }
    // 按 IP 地址连的：回答的那台是谁，现在才知道
    if (viewer->address != address || (!viewer->to.isEmpty() && viewer->to != from)) {
        qWarning("局域网里 %s（设备 %s）冒用了别的连接的编号，不理", qUtf8Printable(address), qUtf8Printable(from));
        return;
    }
    viewer->to = from;
    emit toViewer(from, session, text);
}

void Signaling::declinedByPeer(const QString &from, const QString &session, const QString &reason)
{
    const auto viewer = viewerSessions_.constFind(session);
    if (viewer != viewerSessions_.cend() && (viewer->to.isEmpty() || viewer->to == from))
        emit viewerDeclined(from, session, reason);
}

void Signaling::undeliverable(const QString &to, const QString &session, const QString &reason)
{
    const auto viewer = viewerSessions_.constFind(session);
    if (viewer != viewerSessions_.cend() && viewer->to == to)
        emit viewerUndeliverable(to, session, reason);
}

void Signaling::lanUnreachable(const QString &session)
{
    const auto viewer = viewerSessions_.constFind(session);
    if (viewer != viewerSessions_.cend() && !viewer->address.isEmpty())
        emit viewerUndeliverable(viewer->to, session, rc::protocol::Unreachable);
}

void Signaling::fromSharer(const QString &to, const QString &session, const QString &text)
{
    const Incoming incoming = sharerSessions_.value(session);
    if (incoming.lan)
        emit answerToLan(session, text, incoming.mode);
    else
        emit toServer(to, session, text, incoming.mode);
}

void Signaling::declineFromSharer(const QString &to, const QString &session, const QString &reason)
{
    if (sharerSessions_.value(session).lan)
        emit declineToLan(session, reason);
    else
        emit declineToServer(to, session, reason);
}

void Signaling::endViewer(const QString &session)
{
    if (viewerSessions_.remove(session))
        emit forgotten(session);
}

void Signaling::keepSharer(const QString &session)
{
    const auto incoming = sharerSessions_.find(session);
    if (incoming != sharerSessions_.end())
        incoming->kept = true;
}

void Signaling::releaseSharer(const QString &session)
{
    if (sharerSessions_.remove(session))
        emit forgotten(session);
}

void Signaling::incoming(const QString &session, bool lan, const rc::ConnectionMode &mode)
{
    Incoming &incoming = sharerSessions_[session];
    incoming.lan = lan;
    incoming.mode = mode;
    incoming.expires = QDeadlineTimer(RememberMs);
}

void Signaling::forget()
{
    QStringList expired;
    for (auto it = sharerSessions_.cbegin(); it != sharerSessions_.cend(); ++it) {
        if (!it->kept && it->expires.hasExpired())
            expired.append(it.key());
    }
    for (const QString &session : std::as_const(expired)) {
        sharerSessions_.remove(session);
        emit forgotten(session);
    }
}
