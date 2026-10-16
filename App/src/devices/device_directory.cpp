#include "device_directory.h"

#include "account.h"
#include "service_client.h"

#include <rc/device_id.h>
#include <rc/json_file.h>
#include <rc/server_protocol.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QImage>
#include <QJSEngine>
#include <QJsonArray>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace protocol = rc::protocol;

namespace {

DeviceDirectory *instance = nullptr;

// 按 IP 地址记的那一项的 key（设备 ID 的就是 9 位数字）
QString addressKey(const QString &address)
{
    return QStringLiteral("ip:") + address;
}

} // namespace

QString DeviceDirectory::Entry::key() const
{
    return deviceId.isEmpty() ? addressKey(address) : deviceId;
}

DeviceDirectory::DeviceDirectory(ServiceClient &service, Account &account, QString file, QObject *parent)
    : QObject(parent)
    , service_(service)
    , account_(account)
    , file_(std::move(file))
{
    load();
    // 0.1.3-beta 存过断开那一刻的画面（看得到对方开着的窗口）：改成桌面背景以后不用了，删掉
    QDir(QFileInfo(file_).absolutePath() + QStringLiteral("/thumbnails")).removeRecursively();
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic == protocol::NearbyTopic)
            nearbyChanged();
    });
    connect(&account_, &Account::changed, this, &DeviceDirectory::accountChanged);
}

void DeviceDirectory::setInstance(DeviceDirectory *directory)
{
    instance = directory;
}

DeviceDirectory *DeviceDirectory::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void DeviceDirectory::nearbyChanged()
{
    const auto state = protocol::NearbyState::fromJson(service_.topic(protocol::NearbyTopic));
    if (state == nearby_)
        return;
    nearby_ = state;
    // 记着的设备跟着它现在的名字、系统
    bool known = false;
    for (const protocol::NearbyDevice &device : std::as_const(nearby_.devices)) {
        if (Entry *existing = entry(device.deviceId)) {
            existing->name = device.name;
            existing->os = device.platform;
            known = true;
        }
    }
    if (known)
        store();
    emit changed();
}

// 账号下的手机不列：连不了（rc::server::acceptsConnections），账号页里能看到、能移除
void DeviceDirectory::accountChanged()
{
    QList<protocol::AccountDevice> devices = account_.otherDevices();
    devices.removeIf([](const protocol::AccountDevice &device) { return !rc::server::acceptsConnections(device.platform); });
    if (devices == mine_)
        return;
    mine_ = devices;
    bool known = false;
    for (const protocol::AccountDevice &device : std::as_const(mine_)) {
        if (Entry *existing = entry(device.deviceId)) {
            existing->name = device.name;
            existing->os = device.platform;
            known = true;
        }
    }
    if (known)
        store();
    emit changed();
}

QVariantList DeviceDirectory::devices() const
{
    QVariantList list;
    QSet<QString> listed;
    for (const Entry &entry : entries_) {
        if (entry.empty() && !nearby(entry.deviceId) && !mine(entry.deviceId))
            continue;
        list.append(toVariant(entry));
        listed.insert(entry.deviceId);
    }
    // 没记着的我的设备、附近的设备
    for (const protocol::AccountDevice &device : mine_) {
        if (!listed.contains(device.deviceId)) {
            list.append(toVariant({device.deviceId, {}, {}, device.name, device.platform}));
            listed.insert(device.deviceId);
        }
    }
    for (const protocol::NearbyDevice &device : nearby_.devices) {
        if (!listed.contains(device.deviceId))
            list.append(toVariant({device.deviceId, {}, {}, device.name, device.platform}));
    }
    return list;
}

QVariantMap DeviceDirectory::find(const QString &key) const
{
    const auto found = std::find_if(entries_.cbegin(), entries_.cend(), [&key](const Entry &e) { return e.key() == key; });
    if (found != entries_.cend())
        return toVariant(*found);
    if (const protocol::AccountDevice *device = mine(key))
        return toVariant({device->deviceId, {}, {}, device->name, device->platform});
    if (const protocol::NearbyDevice *device = nearby(key))
        return toVariant({device->deviceId, {}, {}, device->name, device->platform});
    return {};
}

QVariantMap DeviceDirectory::ensure(const QString &text, const QString &alias)
{
    Entry *found = nullptr;
    if (const rc::DeviceId id = rc::DeviceId::parse(text); id.isValid()) {
        found = entryFor(id.digits());
        if (!found) {
            entries_.append({id.digits()});
            found = &entries_.last();
        }
    } else if (const QHostAddress address(text.trimmed()); !address.isNull()) {
        // 附近哪台是这个地址，就是那一台（按设备 ID 记）
        const auto device = std::find_if(nearby_.devices.cbegin(), nearby_.devices.cend(),
                                         [&address](const protocol::NearbyDevice &d) {
                                             return QHostAddress(d.address).isEqual(address);
                                         });
        if (device != nearby_.devices.cend()) {
            found = entryFor(device->deviceId);
        } else {
            found = entry(addressKey(address.toString()));
            if (!found) {
                entries_.append({{}, address.toString()});
                found = &entries_.last();
            }
        }
    } else {
        return {};
    }
    if (!alias.isEmpty())
        found->alias = alias;
    const QString key = found->key();
    store();
    emit changed();
    return this->find(key);
}

void DeviceDirectory::toggleFavorite(const QString &key)
{
    Entry *found = entryFor(key);
    if (!found)
        return;
    found->favorite = !found->favorite;
    store();
    emit changed();
}

void DeviceDirectory::save(const QString &key)
{
    Entry *found = entryFor(key);
    if (!found)
        return;
    found->saved = true;
    store();
    emit changed();
}

void DeviceDirectory::forget(const QString &key)
{
    Entry *found = entry(key);
    if (!found)
        return;
    found->saved = false;
    store();
    emit changed();
}

void DeviceDirectory::touch(const QString &key)
{
    Entry *found = entryFor(key);
    if (!found)
        return;
    found->lastSeen = QDateTime::currentMSecsSinceEpoch();
    store();
    emit changed();
}

void DeviceDirectory::rename(const QString &key, const QString &alias)
{
    Entry *found = entryFor(key);
    if (!found || found->alias == alias.trimmed())
        return;
    found->alias = alias.trimmed();
    store();
    emit changed();
}

void DeviceDirectory::setNote(const QString &key, const QString &note)
{
    Entry *found = entryFor(key);
    if (!found || found->note == note.trimmed())
        return;
    found->note = note.trimmed();
    store();
    emit changed();
}

void DeviceDirectory::setWallpaper(const QString &key, const QByteArray &jpeg)
{
    // 对方发来的：是一张图才存，太大的不要
    constexpr qsizetype MaxBytes = 4 << 20;
    if (jpeg.size() > MaxBytes || QImage::fromData(jpeg, "JPG").isNull())
        return;
    QFile file(wallpaperFile(key));
    if (file.open(QIODevice::ReadOnly) && file.readAll() == jpeg)
        return;
    file.close();
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (!file.open(QIODevice::WriteOnly) || file.write(jpeg) != jpeg.size()) {
        qWarning("存不了 %s 的桌面背景：%s", qUtf8Printable(key), qUtf8Printable(file.errorString()));
        return;
    }
    file.close();
    emit changed();
}

QString DeviceDirectory::wallpaperFile(const QString &key) const
{
    const QByteArray name = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return QFileInfo(file_).absolutePath() + QStringLiteral("/wallpapers/") + QString::fromLatin1(name)
           + QStringLiteral(".jpg");
}

DeviceDirectory::Entry *DeviceDirectory::entry(const QString &key)
{
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&key](const Entry &e) { return e.key() == key; });
    return found == entries_.end() ? nullptr : &*found;
}

DeviceDirectory::Entry *DeviceDirectory::entryFor(const QString &key)
{
    if (Entry *found = entry(key))
        return found;
    if (const protocol::AccountDevice *device = mine(key))
        entries_.append({device->deviceId, {}, {}, device->name, device->platform});
    else if (const protocol::NearbyDevice *device = nearby(key))
        entries_.append({device->deviceId, {}, {}, device->name, device->platform});
    else
        return nullptr;
    return &entries_.last();
}

const protocol::NearbyDevice *DeviceDirectory::nearby(const QString &deviceId) const
{
    const auto found = std::find_if(nearby_.devices.cbegin(), nearby_.devices.cend(),
                                    [&deviceId](const protocol::NearbyDevice &d) { return d.deviceId == deviceId; });
    return found == nearby_.devices.cend() ? nullptr : &*found;
}

const protocol::AccountDevice *DeviceDirectory::mine(const QString &deviceId) const
{
    const auto found = std::find_if(mine_.cbegin(), mine_.cend(),
                                    [&deviceId](const protocol::AccountDevice &d) { return d.deviceId == deviceId; });
    return found == mine_.cend() ? nullptr : &*found;
}

QVariantMap DeviceDirectory::toVariant(const Entry &entry) const
{
    const protocol::NearbyDevice *near = entry.deviceId.isEmpty() ? nullptr : nearby(entry.deviceId);
    const protocol::AccountDevice *own = entry.deviceId.isEmpty() ? nullptr : mine(entry.deviceId);
    const QString deviceId = rc::DeviceId::parse(entry.deviceId).display();
    const QString ip = near ? near->address : entry.address;
    const QString ownName = near                    ? near->name
                          : own                     ? own->name
                          : !entry.name.isEmpty()   ? entry.name
                          : !deviceId.isEmpty()     ? deviceId
                                                    : entry.address;
    const QString name = !entry.alias.isEmpty() ? entry.alias : ownName;
    // 文件换了地址也跟着变（界面上的图片按地址缓存）
    const QFileInfo wallpaper(wallpaperFile(entry.key()));
    QUrl wallpaperUrl;
    if (wallpaper.exists()) {
        wallpaperUrl = QUrl::fromLocalFile(wallpaper.absoluteFilePath());
        wallpaperUrl.setQuery(QString::number(wallpaper.lastModified().toMSecsSinceEpoch()));
    }
    const QString os = near ? near->platform : own ? own->platform : entry.os;
    return {
        {QStringLiteral("key"), entry.key()},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("ip"), ip},
        {QStringLiteral("address"), ip.isEmpty() ? deviceId : ip},
        {QStringLiteral("target"), entry.deviceId.isEmpty() ? entry.address : entry.deviceId},
        {QStringLiteral("name"), name},
        {QStringLiteral("os"), os},
        {QStringLiteral("online"), near != nullptr || (own && own->online)},
        {QStringLiteral("presenceKnown"), near != nullptr || own != nullptr},
        {QStringLiteral("mine"), own != nullptr},
        {QStringLiteral("favorite"), entry.favorite},
        {QStringLiteral("saved"), entry.saved},
        {QStringLiteral("discovered"), near != nullptr},
        {QStringLiteral("lastSeen"), entry.lastSeen},
        {QStringLiteral("paired"), false},
        {QStringLiteral("alias"), entry.alias},
        {QStringLiteral("ownName"), ownName},
        {QStringLiteral("note"), entry.note},
        {QStringLiteral("wallpaper"), wallpaperUrl},
    };
}

void DeviceDirectory::load()
{
    for (const QJsonValue &value : rc::readJsonFile(file_).value(QLatin1StringView("devices")).toArray()) {
        const QJsonObject json = value.toObject();
        Entry entry;
        entry.deviceId = rc::DeviceId::parse(json.value(QLatin1StringView("deviceId")).toString()).digits();
        entry.address = json.value(QLatin1StringView("address")).toString();
        entry.alias = json.value(QLatin1StringView("alias")).toString();
        entry.name = json.value(QLatin1StringView("name")).toString();
        entry.note = json.value(QLatin1StringView("note")).toString();
        entry.os = json.value(QLatin1StringView("os")).toString();
        entry.favorite = json.value(QLatin1StringView("favorite")).toBool();
        entry.saved = json.value(QLatin1StringView("saved")).toBool();
        entry.lastSeen = json.value(QLatin1StringView("lastSeen")).toInteger();
        if (!entry.deviceId.isEmpty() || !entry.address.isEmpty())
            entries_.append(entry);
    }
}

void DeviceDirectory::store()
{
    QJsonArray array;
    for (const Entry &entry : std::as_const(entries_)) {
        if (entry.empty())
            continue;
        array.append(QJsonObject{
            {QStringLiteral("deviceId"), entry.deviceId},
            {QStringLiteral("address"), entry.address},
            {QStringLiteral("alias"), entry.alias},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("note"), entry.note},
            {QStringLiteral("os"), entry.os},
            {QStringLiteral("favorite"), entry.favorite},
            {QStringLiteral("saved"), entry.saved},
            {QStringLiteral("lastSeen"), entry.lastSeen},
        });
    }
    rc::writeJsonFile(file_, {{QStringLiteral("devices"), array}});
}
