#include "sharing.h"

namespace protocol = rc::protocol;
using share_ids::Session;

Sharing::Sharing(DeviceStore &store, rc::InterruptedTransfers &transfers, const ShareCapture &capture,
                 QObject *parent)
    : QObject(parent)
    , others_(rc::PasswordAccess, capture, store, transfers)
    , mine_(rc::DeviceAccess, capture, store, transfers)
{
    for (ShareHost *host : {&others_, &mine_}) {
        connect(host, &ShareHost::answer, this, &Sharing::answer);
        connect(host, &ShareHost::declined, this, &Sharing::declined);
        connect(host, &ShareHost::stateChanged, this, &Sharing::stateChanged);
        connect(host, &ShareHost::viewerConnected, this, [this, host](const protocol::SharePeer &viewer, const QString &session) {
            emit viewerConnected(globalPeer(*host, viewer), session);
        });
        connect(host, &ShareHost::viewerGone, this, &Sharing::viewerGone);
        connect(host, &ShareHost::requestFailed, this, [this, host](const protocol::SharePeer &request, const QString &reason) {
            emit requestFailed(globalPeer(*host, request), reason);
        });
        connect(host, &ShareHost::wrongPin, this, &Sharing::wrongPin);
        connect(host, &ShareHost::wallpaperWanted, this, &Sharing::wallpaperWanted);
        connect(host, &ShareHost::powerDone, this, [this, host](const protocol::SharePeer &viewer, const QString &action) {
            emit powerDone(globalPeer(*host, viewer), action);
        });
        connect(&host->files(), &rc::FileTransfers::changed, this, &Sharing::transfersChanged);
        connect(&host->files(), &rc::FileTransfers::completed, this,
                [this, host](const rc::Transfer &transfer) { emit transferCompleted(globalTransfer(*host, transfer)); });
    }
}

// 别人的那个说本机现在接不接受别人连接、为什么不接受；连着的人两个都算
protocol::ShareState Sharing::state() const
{
    protocol::ShareState state = others_.state();
    for (protocol::SharePeer &request : state.requests)
        request = globalPeer(others_, request);
    for (protocol::SharePeer &viewer : state.viewers)
        viewer = globalPeer(others_, viewer);
    const protocol::ShareState mine = mine_.state();
    for (const protocol::SharePeer &viewer : mine.viewers)
        state.viewers.append(globalPeer(mine_, viewer));
    return state;
}

QList<rc::Transfer> Sharing::transfers() const
{
    QList<rc::Transfer> all;
    for (const ShareHost *host : {&others_, &mine_}) {
        for (const rc::Transfer &transfer : host->files().transfers())
            all.append(globalTransfer(*host, transfer));
    }
    return all;
}

void Sharing::setTrustedKeys(const std::optional<QList<QByteArray>> &keys)
{
    mine_.setTrustedKeys(keys);
}

void Sharing::setAttended(bool attended)
{
    others_.setAttended(attended);
    mine_.setAttended(attended);
}

void Sharing::setIceServers(const rc::IceServers &ice)
{
    others_.setIceServers(ice);
    mine_.setIceServers(ice);
}

// 授权是这台电脑的：问一次就够
void Sharing::requestPermission(const QString &permission)
{
    others_.requestPermission(permission);
}

void Sharing::restart()
{
    others_.restart();
    mine_.restart();
}

void Sharing::webViewer(const QUrl &relay, const QString &ticket)
{
    others_.webViewer(relay, ticket);
}

void Sharing::receiveSignal(const QString &from, const QString &name, const QString &session, const QString &text,
                            const rc::ConnectionMode &mode)
{
    ShareHost &host = mode.access == rc::DeviceAccess ? mine_ : others_;
    host.receiveSignal(from, name, session, text, mode);
}

void Sharing::admit(int id, bool control)
{
    host(id).admit(share_ids::local(id), control);
}

void Sharing::refuse(int id)
{
    host(id).refuse(share_ids::local(id));
}

void Sharing::disconnectViewer(int id)
{
    host(id).disconnectViewer(share_ids::local(id));
}

void Sharing::setControl(int id, bool control)
{
    host(id).setControl(share_ids::local(id), control);
}

void Sharing::disconnectAll()
{
    others_.disconnectAll();
    mine_.disconnectAll();
}

void Sharing::stopForRestart()
{
    others_.stopForRestart();
    mine_.stopForRestart();
}

void Sharing::setClipboard(bool clipboard)
{
    others_.setClipboard(clipboard);
    mine_.setClipboard(clipboard);
}

void Sharing::setWallpaper(const QByteArray &jpeg)
{
    others_.setWallpaper(jpeg);
    mine_.setWallpaper(jpeg);
}

void Sharing::addChannel(std::uint32_t channel, const rc::peer::DataChannels::Handler &handler)
{
    for (ShareHost *host : {&others_, &mine_}) {
        const Session session = sessionOf(*host);
        host->addChannel(channel, [session, handler](int id, const QByteArray &data) {
            handler(share_ids::global(session, id), data);
        });
    }
}

rc::peer::SendResult Sharing::sendData(int id, std::uint32_t channel, const QByteArray &data)
{
    return host(id).sendData(share_ids::local(id), channel, data);
}

void Sharing::sendFiles(int viewer, const QStringList &paths)
{
    host(viewer).files().send(share_ids::local(viewer), paths);
}

void Sharing::acceptTransfer(int id, const QString &directory)
{
    host(id).files().accept(share_ids::local(id), directory);
}

void Sharing::rejectTransfer(int id)
{
    host(id).files().reject(share_ids::local(id));
}

void Sharing::cancelTransfer(int id)
{
    host(id).files().cancel(share_ids::local(id));
}

void Sharing::retryTransfer(int id)
{
    host(id).files().retry(share_ids::local(id));
}

void Sharing::removeTransfer(int id)
{
    host(id).files().remove(share_ids::local(id));
}

Session Sharing::sessionOf(const ShareHost &host) const
{
    return &host == &mine_ ? Session::Mine : Session::Others;
}

protocol::SharePeer Sharing::globalPeer(const ShareHost &host, protocol::SharePeer peer) const
{
    peer.id = share_ids::global(sessionOf(host), peer.id);
    return peer;
}

rc::Transfer Sharing::globalTransfer(const ShareHost &host, rc::Transfer transfer) const
{
    transfer.id = share_ids::global(sessionOf(host), transfer.id);
    transfer.viewer = share_ids::global(sessionOf(host), transfer.viewer);
    return transfer;
}
