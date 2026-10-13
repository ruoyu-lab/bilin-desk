#include "remote_displays.h"

#include <QBuffer>
#include <QImage>
#include <QVariantMap>

#include <algorithm>

namespace control = rc::control;

RemoteDisplays::RemoteDisplays(std::function<void(const QByteArray &)> send, QObject *parent)
    : QObject(parent)
    , send_(std::move(send))
{
}

QVariantList RemoteDisplays::items() const
{
    QVariantList items;
    for (const control::Display &d : list_.displays) {
        items.append(QVariantMap{
            {QStringLiteral("name"), d.name},
            {QStringLiteral("width"), d.width},
            {QStringLiteral("height"), d.height},
            {QStringLiteral("main"), d.main},
            {QStringLiteral("thumbnail"), thumbnails_.value(d.id)},
        });
    }
    return items;
}

int RemoteDisplays::currentIndex() const
{
    const auto current = std::find_if(list_.displays.cbegin(), list_.displays.cend(),
                                      [this](const control::Display &d) { return d.id == list_.current; });
    return current == list_.displays.cend() ? -1 : int(current - list_.displays.cbegin());
}

void RemoteDisplays::received(const QByteArray &message)
{
    if (const std::optional<control::DisplayPicture> picture = control::parseThumbnail(message)) {
        // BGRA8 的字节在小端机器上就是 ARGB32
        const QImage image(reinterpret_cast<const uchar *>(picture->pixels.constData()), picture->width,
                           picture->height, picture->width * 4, QImage::Format_ARGB32);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        thumbnails_.insert(picture->id, QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64()));
        emit changed();
        return;
    }
    std::optional<control::DisplayList> list = control::parseDisplays(message);
    if (!list) {
        qWarning("对方发来不认识的控制消息");
        return;
    }
    if (*list == list_)
        return;
    list_ = std::move(*list);
    emit changed();
}

void RemoteDisplays::clear()
{
    if (list_.displays.isEmpty())
        return;
    list_ = {};
    thumbnails_.clear();
    emit changed();
}

void RemoteDisplays::show(int index)
{
    // 对方换好了会告诉这里（received），那时才算换了
    send_(control::showDisplay(list_.displays.at(index).id));
}

void RemoteDisplays::requestThumbnails()
{
    send_(control::thumbnails());
}
