#pragma once

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QString>

class ServiceClient;

// 本机桌面的背景（电脑版）：有人连进本机时后台服务要一张（WallpaperWanted），这里读主显示器的背景图，缩小成
// JPEG 发过去（SetWallpaper），对方在设备详情顶上显示，不带窗口、图标，看不到本机在做什么。后台服务以 SYSTEM、
// root 运行，读不到登录的人的背景，所以在界面程序里读。几千像素的图解码、缩小要一会儿，在别的线程上做
class Wallpaper : public QObject
{
    Q_OBJECT

public:
    explicit Wallpaper(ServiceClient &service, QObject *parent = nullptr);

private:
    void read();

    ServiceClient &service_;
    bool reading_ = false;
};

// 各系统的（wallpaper_win.cpp、wallpaper_mac.mm）
namespace wallpaper {

// 发过去的图最长的一边（对方的详情页顶上铺满，Retina 上也够清楚）
inline constexpr int MaxSide = 1280;

// 主显示器的背景：图片文件；没有图片、只是纯色时 color（Windows）。在界面线程上取
struct Source
{
    QString file;
    QColor color;
    qreal aspect = 16.0 / 9; // 主显示器的宽高比：纯色时按它画
};
Source source();
// 缩小成 JPEG（最长的一边 MaxSide），哪个线程都行；读不出来时为空
QByteArray jpeg(const Source &source);

} // namespace wallpaper
