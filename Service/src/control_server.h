#pragma once

#include <rc/protocol.h>
#include <rc/transport.h>

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QLocalServer>
#include <QObject>
#include <QSet>
#include <QStringList>

class DeviceStore;

namespace rc {
class LineChannel;
}

// 本机连接的服务端：界面程序连上来看本机的状态、改设置、决定别人能不能连本机（消息见 rc/protocol.h）。
// 连上时把每个主题发一遍，之后有变化发给所有连着的界面。
// device、service 主题由它自己发，其他主题由别处经 publish 交给它
class ControlServer : public QObject
{
    Q_OBJECT

public:
    // 谁能连上来：开发版只有当前用户；装好时（后台服务以 SYSTEM / root 运行）控制台前的用户和管理员，
    // 替他们读写文件以前核对他们自己能不能（client_rights.h）
    enum class Clients { CurrentUser, ConsoleUsers };

    ControlServer(DeviceStore &store, rc::protocol::ServiceInfo info, Clients clients = Clients::CurrentUser,
                  QObject *parent = nullptr);

    // 开始监听 endpoint。调用前要确认没有别的后台服务在用这个名字（main 用锁文件保证）：
    // 上次异常退出留下的套接字文件会被清掉
    bool listen(const QString &endpoint);

    // 主题的新内容：发给所有连着的界面，之后连上的也会收到
    void publish(QLatin1StringView topic, const QJsonObject &value);
    // 一次性的通知，发给所有连着的界面
    void sendEvent(QLatin1StringView event, const QJsonObject &arguments);
    // 连着的界面有几个
    qsizetype clientCount() const { return channels_.size(); }

signals:
    void clientsChanged();
    void quitRequested();
    // 界面程序要退出了
    void appQuittingRequested();
    // 界面里去连别人的会话要发信令（address、mode 见 rc/protocol.h 的 Signal）
    void signalRequested(const QString &to, const QString &address, const QString &session, const QString &text,
                         const rc::ConnectionMode &mode);
    // 界面里的这次连接结束了：界面说的（EndSignal），或者发它的界面程序断开了
    void signalEnded(const QString &session);
    // 界面上的人对别人连本机的决定（id 见 share 主题）
    void admitRequested(int id, bool control);
    void refuseRequested(int id);
    void disconnectRequested(int id);
    void controlRequested(int id, bool control);
    void clipboardRequested(bool clipboard);
    // permission：rc/protocol.h 的 ScreenPermission | InputPermission
    void permissionRequested(const QString &permission);
    void restartShareRequested();
    // 界面上的人对连着本机的人 id 说一句话（ref 见 rc/protocol.h 的 SendChat）
    void chatRequested(int id, int ref, const QString &text);
    // 界面上的人要发验证码、注册、登录、找回密码、退出账号，把账号下的另一台设备移出去
    void sendCodeRequested(const QString &email, const QString &purpose);
    void registerRequested(const QString &email, const QString &password, const QString &nickname, const QString &code);
    void signInRequested(const QString &email, const QString &password);
    void resetPasswordRequested(const QString &email, const QString &code, const QString &password);
    void signOutRequested();
    void removeDeviceRequested(const QString &deviceId);
    // 改昵称、改密码、注销账号
    void nicknameRequested(const QString &nickname);
    void clearHistoryRequested();
    // 界面上点了"更新"（装好的程序才有新版本，Updater）
    void installUpdateRequested();
    // 设置里点了"检查更新"
    void checkUpdateRequested();
    void changePasswordRequested(const QString &oldPassword, const QString &newPassword);
    void deleteAccountRequested(const QString &password);
    // 界面上的人对别人连本机时互传的文件的操作（见 rc/protocol.h 的 SendFiles 等，id 见 files 主题）
    void sendFilesRequested(int viewer, const QStringList &paths);
    void acceptTransferRequested(int id, const QString &directory);
    void rejectTransferRequested(int id);
    void cancelTransferRequested(int id);
    void retryTransferRequested(int id);
    void removeTransferRequested(int id);

private:
    void accept();
    // socket：发来这条命令的那个界面的本机连接
    void handle(qintptr socket, const QJsonObject &message);
    void requestSignal(qintptr socket, const QJsonObject &message);
    void endSignal(qintptr socket, const QString &session);
    void sendFiles(qintptr socket, const QJsonObject &message);
    void acceptTransfer(qintptr socket, const QJsonObject &message);
    void broadcast(const QJsonObject &message);
    QJsonObject deviceMessage() const;

    DeviceStore &store_;
    rc::protocol::ServiceInfo info_;
    const Clients clients_;
    QLocalServer server_;
    QList<rc::LineChannel *> channels_;
    QHash<qintptr, QSet<QString>> signalSessions_; // 每个界面的本机连接发过信令、还没结束的连接
    QHash<QLatin1StringView, QJsonObject> topics_; // publish 交来的（主题名是 rc/protocol.h 里的常量）
};
