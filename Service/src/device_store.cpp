#include "device_store.h"

#include <rc/device_name.h>
#include <rc/json_file.h>
#include <rc/server_protocol.h>
#include <rc/temporary_password.h>
#include <rc/user_seal.h>

#include <QJsonDocument>

namespace {

constexpr QLatin1StringView DeviceIdKey("deviceId");
constexpr QLatin1StringView SecretsKey("secrets");
constexpr QLatin1StringView TokenKey("token");
constexpr QLatin1StringView DeviceKeyKey("deviceKey");

// P-256 的密钥对：0x04 || X || Y || d
bool isDeviceKey(const QByteArray &key)
{
    return key.size() == DeviceStore::DeviceKeySize && key.at(0) == 0x04;
}
constexpr QLatin1StringView DeviceNameKey("deviceName");
constexpr QLatin1StringView AcceptIncomingKey("acceptIncoming");
constexpr QLatin1StringView LanDiscoveryKey("lanDiscovery");
constexpr QLatin1StringView UnattendedKey("unattended");

} // namespace

DeviceStore::DeviceStore(QString directory, std::function<QByteArray()> generateKey, QObject *parent)
    : QObject(parent)
    , directory_(std::move(directory))
    , generateKey_(std::move(generateKey))
    , password_(rc::generateTemporaryPassword())
{
    loadIdentity();
    loadSettings();
}

void DeviceStore::loadIdentity()
{
    const QJsonObject identity = rc::readJsonFile(directory_ + QStringLiteral("/identity.json"));
    deviceId_ = rc::DeviceId::parse(identity.value(DeviceIdKey).toString());
    const std::optional<QByteArray> secrets =
        rc::unsealForUser(QByteArray::fromBase64(identity.value(SecretsKey).toString().toLatin1()));
    if (identity.contains(SecretsKey) && !secrets)
        qWarning("identity.json 里的令牌和设备密钥解不开（别的用户或别的电脑上存的），换新的");
    const QJsonObject secret = secrets ? QJsonDocument::fromJson(*secrets).object() : QJsonObject();
    token_ = secret.value(TokenKey).toString();
    deviceKey_ = QByteArray::fromBase64(secret.value(DeviceKeyKey).toString().toLatin1());
    if (deviceId_.isValid() && rc::server::isDeviceToken(token_) && isDeviceKey(deviceKey_))
        return;
    if (!deviceId_.isValid()) {
        if (!identity.isEmpty())
            qWarning("identity.json 里的设备 ID 不对，换一个新的");
        deviceId_ = rc::DeviceId::generate();
        qInfo("本机的设备 ID：%s", qUtf8Printable(deviceId_.display()));
    }
    if (!rc::server::isDeviceToken(token_))
        token_ = rc::server::generateDeviceToken();
    if (!isDeviceKey(deviceKey_)) {
        deviceKey_ = generateKey_();
        qInfo("生成了本机的设备密钥");
    }
    saveIdentity();
}

void DeviceStore::saveIdentity()
{
    const QJsonObject secret{{TokenKey, token_}, {DeviceKeyKey, QString::fromLatin1(deviceKey_.toBase64())}};
    const QByteArray sealed = rc::sealForUser(QJsonDocument(secret).toJson(QJsonDocument::Compact));
    if (!rc::writeJsonFile(directory_ + QStringLiteral("/identity.json"),
                           {{DeviceIdKey, deviceId_.digits()}, {SecretsKey, QString::fromLatin1(sealed.toBase64())}}))
        qCritical("设备 ID 存不下来：下次启动会换成另一个");
}

void DeviceStore::replaceDeviceId()
{
    const rc::DeviceId previous = deviceId_;
    deviceId_ = rc::DeviceId::generate();
    saveIdentity();
    qInfo("设备 ID %s 已经属于别的设备，换成 %s", qUtf8Printable(previous.display()), qUtf8Printable(deviceId_.display()));
    emit changed();
}

void DeviceStore::takeDeviceId(const QString &digits)
{
    const rc::DeviceId id = rc::DeviceId::parse(digits);
    if (!id.isValid() || id == deviceId_)
        return;
    const rc::DeviceId previous = deviceId_;
    deviceId_ = id;
    saveIdentity();
    qInfo("本机是账号下原来的那台电脑：设备 ID %s 换回 %s", qUtf8Printable(previous.display()), qUtf8Printable(deviceId_.display()));
    emit changed();
}

void DeviceStore::loadSettings()
{
    const QJsonObject settings = rc::readJsonFile(directory_ + QStringLiteral("/settings.json"));
    customName_ = settings.value(DeviceNameKey).toString();
    acceptIncoming_ = settings.value(AcceptIncomingKey).toBool(true);
    lanDiscovery_ = settings.value(LanDiscoveryKey).toBool(true);
    unattended_ = settings.value(UnattendedKey).toBool(true);
}

void DeviceStore::saveSettings()
{
    QJsonObject settings{
        {AcceptIncomingKey, acceptIncoming_},
        {LanDiscoveryKey, lanDiscovery_},
        {UnattendedKey, unattended_},
    };
    if (!customName_.isEmpty())
        settings.insert(DeviceNameKey, customName_);
    rc::writeJsonFile(directory_ + QStringLiteral("/settings.json"), settings);
}

rc::protocol::DeviceState DeviceStore::state() const
{
    rc::protocol::DeviceState state;
    state.deviceId = deviceId_.digits();
    state.deviceName = customName_.isEmpty() ? rc::defaultDeviceName() : customName_;
    state.password = password_;
    state.acceptIncoming = acceptIncoming_;
    state.lanDiscovery = lanDiscovery_;
    state.unattended = unattended_;
    state.deviceKey = deviceKey_;
    return state;
}

void DeviceStore::setDeviceName(const QString &name)
{
    const QString trimmed = name.trimmed().left(rc::server::MaxNameLength);
    if (trimmed == customName_)
        return;
    customName_ = trimmed;
    saveSettings();
    emit changed();
}

void DeviceStore::setAcceptIncoming(bool accept)
{
    if (accept == acceptIncoming_)
        return;
    acceptIncoming_ = accept;
    saveSettings();
    emit changed();
}

void DeviceStore::setLanDiscovery(bool enabled)
{
    if (enabled == lanDiscovery_)
        return;
    lanDiscovery_ = enabled;
    saveSettings();
    emit changed();
}

void DeviceStore::setUnattended(bool unattended)
{
    if (unattended == unattended_)
        return;
    unattended_ = unattended;
    saveSettings();
    emit changed();
}

void DeviceStore::regeneratePassword()
{
    password_ = rc::generateTemporaryPassword();
    emit changed();
}
