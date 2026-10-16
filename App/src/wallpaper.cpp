#include "wallpaper.h"

#include "service_client.h"

#include <rc/protocol.h>

#include <QGuiApplication>
#include <QJsonObject>
#include <QPointer>
#include <QScreen>
#include <QThreadPool>

Wallpaper::Wallpaper(ServiceClient &service, QObject *parent)
    : QObject(parent)
    , service_(service)
{
    connect(&service_, &ServiceClient::eventReceived, this, [this](const QString &name) {
        if (name == rc::protocol::WallpaperWanted)
            read();
    });
}

void Wallpaper::read()
{
    // 读着时又有人连进来：这一张就是给他的
    if (reading_)
        return;
    reading_ = true;
    wallpaper::Source source = wallpaper::source();
    if (const QScreen *screen = QGuiApplication::primaryScreen(); screen && screen->size().height() > 0)
        source.aspect = qreal(screen->size().width()) / screen->size().height();
    QPointer<Wallpaper> self(this);
    QThreadPool::globalInstance()->start([self, source] {
        const QByteArray jpeg = wallpaper::jpeg(source);
        QMetaObject::invokeMethod(qApp, [self, jpeg] {
            if (!self)
                return;
            self->reading_ = false;
            if (!jpeg.isEmpty())
                self->service_.send(rc::protocol::SetWallpaper, {{QStringLiteral("jpeg"), QString::fromLatin1(jpeg.toBase64())}});
        });
    });
}
