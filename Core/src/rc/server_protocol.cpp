#include "rc/server_protocol.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <QSysInfo>

namespace rc::server {

namespace {

constexpr qsizetype TokenBytes = 32;
constexpr qsizetype TokenLength = 43;

} // namespace

QString generateDeviceToken()
{
    QByteArray bytes(TokenBytes, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), TokenBytes / sizeof(quint32));
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool isDeviceToken(QStringView text)
{
    if (text.size() != TokenLength)
        return false;
    for (const QChar c : text) {
        const bool ok = (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'-'
                        || c == u'_';
        if (!ok)
            return false;
    }
    return true;
}

QString platformName()
{
    const QString type = QSysInfo::productType();
    if (type == u"macos")
        return QStringLiteral("macOS");
    if (type == u"windows")
        return QStringLiteral("Windows");
    if (type == u"android")
        return QStringLiteral("Android");
    if (type == u"ios")
        return QStringLiteral("iOS");
    return type;
}

QJsonObject hello(const QString &deviceId, const QString &token, const QByteArray &publicKey, const QString &name,
                  const QString &version, const QString &machine)
{
    return {
        {QStringLiteral("type"), QStringLiteral("hello")},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("token"), token},
        {QStringLiteral("publicKey"), QString::fromLatin1(publicKey.toBase64())},
        {QStringLiteral("name"), name.left(MaxNameLength)},
        {QStringLiteral("platform"), platformName()},
        {QStringLiteral("version"), version},
        {QStringLiteral("machine"), machine},
    };
}

QJsonObject signal(const QString &to, const QString &session, const QString &text, const rc::ConnectionMode &mode)
{
    QJsonObject message{
        {QStringLiteral("type"), QStringLiteral("signal")},
        {QStringLiteral("to"), to},
        {QStringLiteral("session"), session},
        {QStringLiteral("text"), text},
    };
    mode.write(message);
    return message;
}

QJsonObject decline(const QString &to, const QString &session, const QString &reason)
{
    return {
        {QStringLiteral("type"), QStringLiteral("decline")},
        {QStringLiteral("to"), to},
        {QStringLiteral("session"), session},
        {QStringLiteral("reason"), reason},
    };
}

QJsonObject rename(const QString &name)
{
    return {
        {QStringLiteral("type"), QStringLiteral("rename")},
        {QStringLiteral("name"), name.left(MaxNameLength)},
    };
}

QJsonObject sendCode(const QString &email, const QString &purpose)
{
    return {
        {QStringLiteral("type"), QStringLiteral("sendCode")},
        {QStringLiteral("email"), email},
        {QStringLiteral("purpose"), purpose},
    };
}

QJsonObject registerAccount(const QString &email, const QString &password, const QString &nickname, const QString &code)
{
    return {
        {QStringLiteral("type"), QStringLiteral("register")},
        {QStringLiteral("email"), email},
        {QStringLiteral("password"), password},
        {QStringLiteral("nickname"), nickname},
        {QStringLiteral("code"), code},
    };
}

QJsonObject signIn(const QString &email, const QString &password)
{
    return {
        {QStringLiteral("type"), QStringLiteral("login")},
        {QStringLiteral("email"), email},
        {QStringLiteral("password"), password},
    };
}

QJsonObject resetPassword(const QString &email, const QString &code, const QString &password)
{
    return {
        {QStringLiteral("type"), QStringLiteral("resetPassword")},
        {QStringLiteral("email"), email},
        {QStringLiteral("code"), code},
        {QStringLiteral("password"), password},
    };
}

QJsonObject signOut()
{
    return {{QStringLiteral("type"), QStringLiteral("logout")}};
}

QJsonObject removeDevice(const QString &deviceId)
{
    return {
        {QStringLiteral("type"), QStringLiteral("removeDevice")},
        {QStringLiteral("deviceId"), deviceId},
    };
}

QJsonObject setNickname(const QString &nickname)
{
    return {
        {QStringLiteral("type"), QStringLiteral("setNickname")},
        {QStringLiteral("nickname"), nickname},
    };
}

QJsonObject changePassword(const QString &oldPassword, const QString &newPassword)
{
    return {
        {QStringLiteral("type"), QStringLiteral("changePassword")},
        {QStringLiteral("oldPassword"), oldPassword},
        {QStringLiteral("newPassword"), newPassword},
    };
}

QJsonObject deleteAccount(const QString &password)
{
    return {
        {QStringLiteral("type"), QStringLiteral("deleteAccount")},
        {QStringLiteral("password"), password},
    };
}

} // namespace rc::server
