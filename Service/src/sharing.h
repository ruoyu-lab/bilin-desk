#pragma once

#include "share_host.h"
#include "share_ids.h"

#include <array>

#include <QByteArray>
#include <QUrl>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// 别人连本机：三个分享会话（ShareHost），一次分享只认一种身份（SDK）——
// - 别人：临时密码，本机的人点允许（网页版里没登录本机账号的浏览器也是）
// - 我的设备：账号下别的设备，只靠设备密钥，不要密码、不用点允许（setTrustedKeys）
// - 网页版的我的设备：登录了本机账号的浏览器，服务器给这次连接发的一次性钥匙，不要密码、不用点允许；只为它开着
// 对界面（share、files 主题）和后台服务的其余部分（对话、连接记录、看着界面程序）是一个：来连的信令按它说的
// 怎么证明自己（mode.access）交给对应的那个，连着的人、互传的文件合在一起。编号见 share_ids.h
class Sharing : public QObject
{
    Q_OBJECT

public:
    // capture：采集、声音、键鼠在哪里做（开发版、装好的 Windows、装好的 macOS 不一样）
    Sharing(DeviceStore &store, rc::InterruptedTransfers &transfers, const ShareCapture &capture,
            QObject *parent = nullptr);

    rc::protocol::ShareState state() const;
    // files 主题：两个会话互传的文件
    QList<rc::Transfer> transfers() const;

    // 账号下别的设备的公钥（ServerLink::trustedKeys）
    void setTrustedKeys(const std::optional<QList<QByteArray>> &keys);
    // 下面的和 ShareHost 的一样，id、viewer 是合成的编号
    void setAttended(bool attended);
    void setIceServers(const rc::IceServers &ice);
    void requestPermission(const QString &permission);
    void restart();
    // 网页版来连（ShareHost::webViewer）：带着钥匙的交给网页版的我的设备那一个，不然交给别人的
    void webViewer(const QUrl &relay, const QString &ticket, const QByteArray &key);
    void receiveSignal(const QString &from, const QString &name, const QString &session, const QString &text,
                       const rc::ConnectionMode &mode);
    void admit(int id, bool control);
    void refuse(int id);
    void disconnectViewer(int id);
    void setControl(int id, bool control);
    void disconnectAll();
    void stopForRestart();
    void setClipboard(bool clipboard);
    void setWallpaper(const QByteArray &jpeg);
    void addChannel(std::uint32_t channel, const rc::peer::DataChannels::Handler &handler);
    rc::peer::SendResult sendData(int id, std::uint32_t channel, const QByteArray &data);
    // 互传文件（rc::FileTransfers 的）
    void sendFiles(int viewer, const QStringList &paths);
    void acceptTransfer(int id, const QString &directory);
    void rejectTransfer(int id);
    void cancelTransfer(int id);
    void retryTransfer(int id);
    void removeTransfer(int id);

signals:
    // 和 ShareHost 的一样
    void answer(const QString &to, const QString &session, const QString &text);
    void declined(const QString &to, const QString &session, const QString &reason);
    void stateChanged();
    void viewerConnected(const rc::protocol::SharePeer &viewer, const QString &session);
    void viewerGone(const QString &device, const QString &session);
    void requestFailed(const rc::protocol::SharePeer &request, const QString &reason);
    void wrongPin();
    void powerDone(const rc::protocol::SharePeer &viewer, const QString &action);
    void wallpaperWanted();
    void webAnswered(const QString &ticket, const QString &reason);
    // 互传的文件变了；传完了一份
    void transfersChanged();
    void transferCompleted(const rc::Transfer &transfer);

private:
    ShareHost &host(int id);
    // 三个都要做的
    std::array<ShareHost *, 3> hosts() { return {&others_, &mine_, &webMine_}; }
    std::array<const ShareHost *, 3> hosts() const { return {&others_, &mine_, &webMine_}; }
    share_ids::Session sessionOf(const ShareHost &host) const;
    rc::protocol::SharePeer globalPeer(const ShareHost &host, rc::protocol::SharePeer peer) const;
    rc::Transfer globalTransfer(const ShareHost &host, rc::Transfer transfer) const;

    ShareHost others_;
    ShareHost mine_;
    ShareHost webMine_;
};
