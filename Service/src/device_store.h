#pragma once

#include <rc/device_id.h>
#include <rc/protocol.h>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

// 本机：设备 ID、名字、临时密码和被控那一面的设置。存在 directory 里：
//   identity.json  {"deviceId": "482915307", "secrets": ...}
//                  第一次运行时生成。ID 只在服务器说它已经属于别的设备时换。
//                  secrets 是 {"token": ..., "deviceKey": ...}，存的时候加密（rc/user_seal.h）：
//                  令牌证明"这个 ID 是我的"（rc/server_protocol.h），不给任何人看；
//                  设备密钥（SDK 的 P-256 密钥对，97 字节的 base64：公钥 65 字节在前，私钥在后）证明"我是这台设备"：
//                  公钥交给服务器，同一账号的设备、信任本机的设备用它认出本机；私钥不给任何人
//   settings.json  {"deviceName": ..., "acceptIncoming": ..., "lanDiscovery": ..., "unattended": ..., "connectionStats": ...}
//                  deviceName 只在用户起过名字时有；没有时用本机默认的名字，跟着系统里的电脑名称变
//   trust.json     rc::protocol::TrustState：本机信任的别人的设备、信任本机的设备（docs/devices-and-accounts.md）
// 临时密码不存：每次启动换一个
class DeviceStore : public QObject
{
    Q_OBJECT

public:
    // generateKey：没有设备密钥时生成一个（SDK 的 rcsdk::generateDeviceKey；测试里给假的）
    DeviceStore(QString directory, std::function<QByteArray()> generateKey, QObject *parent = nullptr);

    static constexpr qsizetype DeviceKeySize = 97;
    static constexpr qsizetype PublicKeySize = 65;

    rc::protocol::DeviceState state() const;
    QString deviceToken() const { return token_; }
    QByteArray deviceKey() const { return deviceKey_; }
    QByteArray publicKey() const { return deviceKey_.left(PublicKeySize); }

    // 空的（或只有空格）：用回默认的名字。最长 64 个字（服务器的上限），多的不要
    void setDeviceName(const QString &name);
    void setAcceptIncoming(bool accept);
    void setLanDiscovery(bool enabled);
    void setUnattended(bool unattended);
    void setConnectionStats(bool enabled);
    void regeneratePassword();
    // 服务器说这个 ID 已经属于别的设备：换一个新的
    void replaceDeviceId();
    // 信任的设备（trustChanged）。本机信任 device（同一个 ID 的换掉，新的在前）；公钥不对的不要
    rc::protocol::TrustState trust() const { return trust_; }
    std::optional<rc::protocol::TrustedDevice> trusted(const QString &deviceId) const;
    void addTrusted(const rc::protocol::TrustedDevice &device);
    void setTrustAsk(const QString &deviceId, bool ask);
    void untrust(const QString &deviceId);
    // 设备 deviceId 信任了本机，以后去连它只认 publicKey（65 字节）；不再信任了就忘掉
    void rememberTrust(const QString &deviceId, const QByteArray &publicKey);
    void forgetTrust(const QString &deviceId);
    static bool isPublicKey(const QByteArray &key);

    // 服务器认出本机是账号下原来的那一台（重装过，rc/machine.h）：换回原来的 ID，令牌、密钥不变。ID 不对时不换
    void takeDeviceId(const QString &digits);

signals:
    void changed();
    void trustChanged();

private:
    void loadIdentity();
    void saveIdentity();
    void loadSettings();
    void saveSettings();
    void loadTrust();
    void saveTrust();

    QString directory_;
    rc::DeviceId deviceId_;
    QString token_;
    QByteArray deviceKey_;
    std::function<QByteArray()> generateKey_;
    QString customName_;
    QString password_;
    bool acceptIncoming_ = true;
    bool lanDiscovery_ = true;
    bool unattended_ = true;
    bool connectionStats_ = true;
    rc::protocol::TrustState trust_;
};
