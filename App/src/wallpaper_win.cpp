#include "wallpaper.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>

#include <windows.h>

namespace wallpaper {

namespace {

QByteArray encode(const QImage &image)
{
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPG", 85))
        return {};
    return jpeg;
}

} // namespace

Source source()
{
    // 幻灯片放映、Windows 聚焦时是正在显示的那一张（系统转好的 TranscodedWallpaper）；只用纯色时为空
    wchar_t path[MAX_PATH] = {};
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0);
    const COLORREF color = GetSysColor(COLOR_DESKTOP);
    return {QString::fromWCharArray(path), QColor(GetRValue(color), GetGValue(color), GetBValue(color))};
}

QByteArray jpeg(const Source &source)
{
    if (!source.file.isEmpty()) {
        // TranscodedWallpaper 没有扩展名：按内容认
        QImageReader reader(source.file);
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (size.isValid() && std::max(size.width(), size.height()) > MaxSide)
            reader.setScaledSize(size.scaled(MaxSide, MaxSide, Qt::KeepAspectRatio));
        if (const QImage image = reader.read(); !image.isNull())
            return encode(image);
    }
    QImage solid(MaxSide, qRound(MaxSide / source.aspect), QImage::Format_RGB32);
    solid.fill(source.color);
    return encode(solid);
}

} // namespace wallpaper
