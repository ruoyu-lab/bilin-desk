#include "account.h"

#include "service_client.h"

#include <rc/device_id.h>

#include <rcsdk/rcsdk.h>

#include <QJSEngine>

namespace protocol = rc::protocol;

namespace {

Account *instance = nullptr;

// signingIn：登录、注册时密码不对说"邮箱或密码不对"，改密码、注销时只说密码；sendingCode：在发验证码
QString failureMessage(const QString &reason, bool signingIn, bool sendingCode)
{
    if (reason == u"emailTaken")
        return Account::tr("这个邮箱已经注册过了");
    if (reason == u"noAccount")
        return Account::tr("这个邮箱还没有注册过");
    if (reason == u"wrongPassword")
        return signingIn ? Account::tr("邮箱或密码不对") : Account::tr("密码不对");
    if (reason == u"wrongCode")
        return Account::tr("验证码不对或者过期了，重新发一个试试");
    if (reason == u"tooMany")
        return sendingCode ? Account::tr("验证码要得太频繁了，过一会儿再试") : Account::tr("试错太多次了，过几分钟再试");
    if (reason == u"mailLimit")
        return Account::tr("今天发出的验证码太多了，明天再试");
    if (reason == u"mailFailed")
        return Account::tr("验证码邮件没发出去，过一会儿再试");
    if (reason == protocol::NoServer)
        return Account::tr("没连上服务器，过一会儿再试");
    return Account::tr("没成功（%1）").arg(reason);
}

} // namespace

Account::Account(ServiceClient &service, QObject *parent)
    : QObject(parent)
    , service_(service)
{
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        // 本机的设备 ID 变了也要重新挑出"别的设备"
        if (topic != protocol::AccountTopic && topic != protocol::DeviceTopic)
            return;
        const auto state = protocol::AccountState::fromJson(service_.topic(protocol::AccountTopic));
        const bool signedInNow = state.signedIn && !state_.signedIn;
        const bool signedOutNow = !state.signedIn && state_.signedIn;
        state_ = state;
        emit changed();
        if (signedInNow && pending_ == Pending::SignIn) {
            setPending(Pending::None);
            setLoginRequested(false);
        } else if (signedOutNow && pending_ == Pending::DeleteAccount) {
            setPending(Pending::None);
        }
    });
    connect(&service_, &ServiceClient::eventReceived, this, [this](const QString &name, const QJsonObject &arguments) {
        if (name == protocol::PasswordChanged && pending_ == Pending::ChangePassword) {
            setPending(Pending::None);
            emit passwordChanged();
        } else if (name == protocol::CodeSent && pending_ == Pending::SendCode) {
            setPending(Pending::None);
            emit codeSent(arguments.value(QLatin1StringView("purpose")).toString());
        } else if (name == protocol::AccountFailed) {
            const bool signingIn = pending_ == Pending::SignIn;
            const bool sendingCode = pending_ == Pending::SendCode;
            setPending(Pending::None);
            emit failed(failureMessage(arguments.value(QLatin1StringView("reason")).toString(), signingIn, sendingCode));
        }
    });
    // 等回答的时候后台服务断了：回答不会来了
    connect(&service_, &ServiceClient::stateChanged, this, [this] {
        if (pending_ != Pending::None && service_.state() != ServiceClient::State::Ready) {
            setPending(Pending::None);
            emit failed(tr("后台服务没有在运行"));
        }
    });
}

void Account::setInstance(Account *account)
{
    instance = account;
}

Account *Account::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

QList<protocol::AccountDevice> Account::otherDevices() const
{
    const QString self = service_.topic(protocol::DeviceTopic).value(QLatin1StringView("deviceId")).toString();
    QList<protocol::AccountDevice> devices;
    for (const protocol::AccountDevice &device : state_.devices) {
        if (device.deviceId != self)
            devices.append(device);
    }
    return devices;
}

QByteArray Account::publicKey(const QString &deviceId) const
{
    for (const protocol::AccountDevice &device : otherDevices()) {
        if (device.deviceId != deviceId)
            continue;
        const QByteArray key = QByteArray::fromBase64(device.publicKey.toLatin1());
        return key.size() == RCS_PUBLIC_KEY_SIZE ? key : QByteArray();
    }
    return {};
}

QVariantList Account::devices() const
{
    QVariantList list;
    for (const protocol::AccountDevice &device : otherDevices()) {
        list.append(QVariantMap{
            {QStringLiteral("deviceId"), rc::DeviceId::parse(device.deviceId).display()},
            {QStringLiteral("name"), device.name},
            {QStringLiteral("platform"), device.platform},
            {QStringLiteral("online"), device.online},
        });
    }
    return list;
}

void Account::setLoginRequested(bool requested)
{
    if (requested == loginRequested_)
        return;
    loginRequested_ = requested;
    emit loginRequestedChanged();
}

void Account::sendCode(const QString &email, const QString &purpose)
{
    request(Pending::SendCode, protocol::SendCode, {{QStringLiteral("email"), email}, {QStringLiteral("purpose"), purpose}});
}

void Account::signIn(const QString &email, const QString &password)
{
    request(Pending::SignIn, protocol::SignIn,
            {{QStringLiteral("email"), email}, {QStringLiteral("password"), password}});
}

void Account::registerAccount(const QString &nickname, const QString &email, const QString &password,
                              const QString &code)
{
    request(Pending::SignIn, protocol::RegisterAccount,
            {{QStringLiteral("email"), email}, {QStringLiteral("password"), password}, {QStringLiteral("nickname"), nickname},
             {QStringLiteral("code"), code}});
}

void Account::resetPassword(const QString &email, const QString &code, const QString &password)
{
    request(Pending::SignIn, protocol::ResetPassword,
            {{QStringLiteral("email"), email}, {QStringLiteral("code"), code}, {QStringLiteral("password"), password}});
}

void Account::changePassword(const QString &oldPassword, const QString &newPassword)
{
    request(Pending::ChangePassword, protocol::ChangePassword,
            {{QStringLiteral("oldPassword"), oldPassword}, {QStringLiteral("newPassword"), newPassword}});
}

void Account::deleteAccount(const QString &password)
{
    request(Pending::DeleteAccount, protocol::DeleteAccount, {{QStringLiteral("password"), password}});
}

void Account::setNickname(const QString &nickname)
{
    service_.send(protocol::SetNickname, {{QStringLiteral("nickname"), nickname}});
}

void Account::request(Pending pending, QLatin1StringView command, const QJsonObject &arguments)
{
    if (service_.state() != ServiceClient::State::Ready) {
        emit failed(tr("后台服务没有在运行"));
        return;
    }
    setPending(pending);
    service_.send(command, arguments);
}

void Account::signOut()
{
    service_.send(protocol::SignOut);
}

void Account::removeDevice(const QString &deviceId)
{
    service_.send(protocol::RemoveDevice, {{QStringLiteral("deviceId"), rc::DeviceId::parse(deviceId).digits()}});
}

void Account::setPending(Pending pending)
{
    if (pending == pending_)
        return;
    const bool wasBusy = busy();
    pending_ = pending;
    if (busy() != wasBusy)
        emit busyChanged();
}
