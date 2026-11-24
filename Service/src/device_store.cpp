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
constexpr QLatin1StringView ConnectionStatsKey("connectionStats");

} // namespace

DeviceStore::DeviceStore(QString directory, std::function<QByteArray()> generateKey, QObject *parent)
    : QObject(parent)
    , directory_(std::move(directory))
    , generateKey_(std::move(generateKey))
    , password_(rc::generateTemporaryPassword())
{
    loadIdentity();
    loadSettings();
    loadTrust();
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
    connectionStats_ = settings.value(ConnectionStatsKey).toBool(true);
}

void DeviceStore::saveSettings()
{
    QJsonObject settings{
        {AcceptIncomingKey, acceptIncoming_},
        {LanDiscoveryKey, lanDiscovery_},
        {UnattendedKey, unattended_},
        {ConnectionStatsKey, connectionStats_},
    };
    if (!customName_.isEmpty())
        settings.insert(DeviceNameKey, customName_);
    rc::writeJsonFile(directory_ + QStringLiteral("/settings.json"), settings);
}

bool DeviceStore::isPublicKey(const QByteArray &key)
{
    return key.size() == PublicKeySize && key.at(0) == 0x04;
}

void DeviceStore::loadTrust()
{
    const auto loaded = rc::protocol::TrustState::fromJson(rc::readJsonFile(directory_ + QStringLiteral("/trust.json")));
    const auto valid = [](const QString &id, const QString &key) {
        return rc::DeviceId::parse(id).isValid() && isPublicKey(QByteArray::fromBase64(key.toLatin1()));
    };
    for (const auto &device : loaded.trusted) {
        if (valid(device.deviceId, device.publicKey))
            trust_.trusted.append(device);
    }
    for (const auto &device : loaded.trustedBy) {
        if (valid(device.deviceId, device.publicKey))
            trust_.trustedBy.append(device);
    }
}

void DeviceStore::saveTrust()
{
    rc::writeJsonFile(directory_ + QStringLiteral("/trust.json"), trust_.toJson());
    emit trustChanged();
}

std::optional<rc::protocol::TrustedDevice> DeviceStore::trusted(const QString &deviceId) const
{
    for (const auto &device : trust_.trusted) {
        if (device.deviceId == deviceId)
            return device;
    }
    return std::nullopt;
}

void DeviceStore::addTrusted(const rc::protocol::TrustedDevice &device)
{
    if (!rc::DeviceId::parse(device.deviceId).isValid() || !isPublicKey(QByteArray::fromBase64(device.publicKey.toLatin1())))
        return;
    trust_.trusted.removeIf([&](const auto &d) { return d.deviceId == device.deviceId; });
    trust_.trusted.prepend(device);
    qInfo("信任了设备 %s（%s）%s", qUtf8Printable(device.deviceId), qUtf8Printable(device.name),
          device.ask ? "，来连时还要点允许" : "，来连时不用点允许");
    saveTrust();
}

void DeviceStore::setTrustAsk(const QString &deviceId, bool ask)
{
    for (auto &device : trust_.trusted) {
        if (device.deviceId == deviceId && device.ask != ask) {
            device.ask = ask;
            saveTrust();
            return;
        }
    }
}

void DeviceStore::untrust(const QString &deviceId)
{
    if (trust_.trusted.removeIf([&](const auto &d) { return d.deviceId == deviceId; }) == 0)
        return;
    qInfo("不再信任设备 %s", qUtf8Printable(deviceId));
    saveTrust();
}

void DeviceStore::rememberTrust(const QString &deviceId, const QByteArray &publicKey)
{
    if (!rc::DeviceId::parse(deviceId).isValid() || !isPublicKey(publicKey))
        return;
    const rc::protocol::TrustingDevice device{deviceId, QString::fromLatin1(publicKey.toBase64())};
    if (trust_.trustedBy.contains(device))
        return;
    trust_.trustedBy.removeIf([&](const auto &d) { return d.deviceId == deviceId; });
    trust_.trustedBy.append(device);
    qInfo("设备 %s 信任了本机：以后去连它不要密码", qUtf8Printable(deviceId));
    saveTrust();
}

void DeviceStore::forgetTrust(const QString &deviceId)
{
    if (trust_.trustedBy.removeIf([&](const auto &d) { return d.deviceId == deviceId; }) == 0)
        return;
    qInfo("设备 %s 不再信任本机", qUtf8Printable(deviceId));
    saveTrust();
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
    state.connectionStats = connectionStats_;
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

void DeviceStore::setConnectionStats(bool enabled)
{
    if (enabled == connectionStats_)
        return;
    connectionStats_ = enabled;
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
