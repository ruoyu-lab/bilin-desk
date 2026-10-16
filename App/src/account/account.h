#pragma once

#include <rc/protocol.h>

#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 本机登录的账号（QML 单例 Account，后台服务的 account 主题）。不登录也能用（按设备 ID 连、局域网）；
// 登录以后，同一个账号下的设备互为"我的设备"，能看到它们在不在线（docs/devices-and-accounts.md）。
// 注册、登录、退出都是给后台服务发命令：成功了 account 主题会变，没成功 failed 带着给人看的说明。
// 注册、找回密码要先往邮箱发验证码（sendCode，发了 codeSent），再带着它注册、重设密码
class Account : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    // 服务器说过登没登录了：刚打开时后台服务还没连上服务器，在这之前 signedIn 还不算数
    Q_PROPERTY(bool known READ known NOTIFY changed)
    Q_PROPERTY(QString email READ email NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    // 账号下除了本机的设备：{ deviceId（三位一组）, name, platform, online }
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    // 在等服务器回答发验证码、注册、登录、找回密码、改密码、注销
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // 有人要打开登录窗口（账号弹窗里点了"登录"）；关掉登录窗口时放回 false
    Q_PROPERTY(bool loginRequested READ loginRequested WRITE setLoginRequested NOTIFY loginRequestedChanged)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个
    Account(ServiceClient &service, QObject *parent);

    static void setInstance(Account *instance);
    static Account *create(QQmlEngine *, QJSEngine *);

    bool signedIn() const { return state_.signedIn; }
    bool known() const { return state_.known; }
    QString email() const { return state_.email; }
    QString displayName() const { return state_.nickname; }
    QVariantList devices() const;
    bool busy() const { return pending_ != Pending::None; }
    bool loginRequested() const { return loginRequested_; }
    void setLoginRequested(bool requested);

    // 账号下除了本机的设备
    QList<rc::protocol::AccountDevice> otherDevices() const;
    // 账号下别的设备 deviceId 的公钥（65 字节）：去连它时按我的设备连（只靠设备密钥）。不是我的设备时为空
    QByteArray publicKey(const QString &deviceId) const;

    // 往邮箱发验证码：register（注册）| resetPassword（找回密码）
    Q_INVOKABLE void sendCode(const QString &email, const QString &purpose);
    Q_INVOKABLE void signIn(const QString &email, const QString &password);
    // code：发到邮箱的验证码
    Q_INVOKABLE void registerAccount(const QString &nickname, const QString &email, const QString &password,
                                     const QString &code);
    // 忘了密码：凭验证码设新密码，成了就登录（同 signIn）
    Q_INVOKABLE void resetPassword(const QString &email, const QString &code, const QString &password);
    Q_INVOKABLE void signOut();
    // deviceId：三位一组也行
    Q_INVOKABLE void removeDevice(const QString &deviceId);
    // 登录着时：改昵称（成了 displayName 跟着变）、改密码（成了 passwordChanged）、注销账号（成了 signedIn 变成 false）
    Q_INVOKABLE void setNickname(const QString &nickname);
    Q_INVOKABLE void changePassword(const QString &oldPassword, const QString &newPassword);
    Q_INVOKABLE void deleteAccount(const QString &password);

signals:
    void changed();
    void busyChanged();
    void loginRequestedChanged();
    void failed(const QString &message);
    void passwordChanged();
    // 验证码发到邮箱了；purpose 同 sendCode
    void codeSent(const QString &purpose);

private:
    // 在等服务器回答的是哪一件
    enum class Pending { None, SendCode, SignIn, ChangePassword, DeleteAccount };

    void request(Pending pending, QLatin1StringView command, const QJsonObject &arguments);
    void setPending(Pending pending);

    ServiceClient &service_;
    rc::protocol::AccountState state_;
    Pending pending_ = Pending::None;
    bool loginRequested_ = false;
};
