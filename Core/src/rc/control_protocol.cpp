#include "rc/control_protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rc::control {

namespace {

// 是 type 这种消息时它的内容
std::optional<QJsonObject> parse(const QByteArray &message, QLatin1StringView type)
{
    const QJsonObject json = QJsonDocument::fromJson(message).object();
    if (json.value(QLatin1StringView("type")).toString() != type)
        return std::nullopt;
    return json;
}

} // namespace

QByteArray displays(const DisplayList &list)
{
    QJsonArray array;
    for (const Display &display : list.displays) {
        array.append(QJsonObject{
            {QStringLiteral("id"), qint64(display.id)},
            {QStringLiteral("name"), display.name},
            {QStringLiteral("width"), display.width},
            {QStringLiteral("height"), display.height},
            {QStringLiteral("main"), display.main},
        });
    }
    const QJsonObject message{
        {QStringLiteral("type"), QString(Displays)},
        {QStringLiteral("displays"), array},
        {QStringLiteral("current"), qint64(list.current)},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray showDisplay(quint32 id)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(ShowDisplay)},
        {QStringLiteral("id"), qint64(id)},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray mainViewer(bool main)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(MainViewer)},
        {QStringLiteral("main"), main},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray sound(QLatin1StringView problem)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(Sound)},
        {QStringLiteral("problem"), QString(problem)},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray encoders(quint32 hardware)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(Encoders)},
        {QStringLiteral("hardware"), qint64(hardware)},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray lowLatency(bool on)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(LowLatency)},
        {QStringLiteral("on"), on},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

QByteArray thumbnails()
{
    return QJsonDocument(QJsonObject{{QStringLiteral("type"), QString(Thumbnails)}}).toJson(QJsonDocument::Compact);
}

QByteArray thumbnail(const DisplayPicture &picture)
{
    const QJsonObject message{
        {QStringLiteral("type"), QString(Thumbnail)},
        {QStringLiteral("id"), qint64(picture.id)},
        {QStringLiteral("width"), picture.width},
        {QStringLiteral("height"), picture.height},
        {QStringLiteral("pixels"), QString::fromLatin1(qCompress(picture.pixels).toBase64())},
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

std::optional<DisplayList> parseDisplays(const QByteArray &message)
{
    const auto json = parse(message, Displays);
    if (!json)
        return std::nullopt;
    DisplayList list;
    for (const QJsonValue &value : json->value(QLatin1StringView("displays")).toArray()) {
        const QJsonObject display = value.toObject();
        list.displays.append({
            quint32(display.value(QLatin1StringView("id")).toInteger()),
            display.value(QLatin1StringView("name")).toString(),
            display.value(QLatin1StringView("width")).toInt(),
            display.value(QLatin1StringView("height")).toInt(),
            display.value(QLatin1StringView("main")).toBool(),
        });
    }
    list.current = quint32(json->value(QLatin1StringView("current")).toInteger());
    return list;
}

std::optional<quint32> parseShowDisplay(const QByteArray &message)
{
    const auto json = parse(message, ShowDisplay);
    if (!json)
        return std::nullopt;
    return quint32(json->value(QLatin1StringView("id")).toInteger());
}

std::optional<bool> parseMainViewer(const QByteArray &message)
{
    const auto json = parse(message, MainViewer);
    if (!json)
        return std::nullopt;
    return json->value(QLatin1StringView("main")).toBool();
}

std::optional<QString> parseSound(const QByteArray &message)
{
    const auto json = parse(message, Sound);
    if (!json)
        return std::nullopt;
    return json->value(QLatin1StringView("problem")).toString();
}

std::optional<quint32> parseEncoders(const QByteArray &message)
{
    const auto json = parse(message, Encoders);
    if (!json)
        return std::nullopt;
    return quint32(json->value(QLatin1StringView("hardware")).toInteger());
}

std::optional<bool> parseLowLatency(const QByteArray &message)
{
    const auto json = parse(message, LowLatency);
    if (!json)
        return std::nullopt;
    return json->value(QLatin1StringView("on")).toBool();
}

bool isThumbnails(const QByteArray &message)
{
    return parse(message, Thumbnails).has_value();
}

std::optional<DisplayPicture> parseThumbnail(const QByteArray &message)
{
    const auto json = parse(message, Thumbnail);
    if (!json)
        return std::nullopt;
    rc::control::DisplayPicture picture;
    picture.id = quint32(json->value(QLatin1StringView("id")).toInteger());
    picture.width = json->value(QLatin1StringView("width")).toInt();
    picture.height = json->value(QLatin1StringView("height")).toInt();
    picture.pixels = qUncompress(QByteArray::fromBase64(json->value(QLatin1StringView("pixels")).toString().toLatin1()));
    if (picture.width <= 0 || picture.height <= 0 || picture.pixels.size() != qsizetype(picture.width) * picture.height * 4)
        return std::nullopt;
    return picture;
}

} // namespace rc::control
