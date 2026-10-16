#pragma once

#include <QByteArray>
#include <QLatin1StringView>
#include <QList>
#include <QString>

#include <optional>

// 两台机器之间数据通道上会话的控制消息（rc/peer_protocol.h 的 ControlChannel），一条消息是一个 JSON 对象，
// type 说明是哪一种。
// 显示器：SDK 只在分享端列得出显示器，观看端靠这些消息知道对方有哪几块、换着看：
// - 分享端 → 观看端 {"type": "displays", "displays": [{"id", "name", "width", "height", "main"}...], "current"}：
//   观看端连进来时、显示器变了、换了一块分享时发
// - 观看端 → 分享端 {"type": "showDisplay", "id"}：要看这一块
// 画面：
// - 分享端 → 观看端 {"type": "mainViewer", "main"}：画面（画质、尺寸）听不听它的。所有人看同一个画面，
//   SDK 按先连上的那个人要的出（主观看端），后连上的改画质不起作用；连进来时、先连上的人走了时发
// 声音：
// - 分享端 → 观看端 {"type": "sound", "problem"}：分享端录不了声音时为什么（SoundNoDevice …），好了时 problem
//   是空的。SDK 只在分享端知道录不了，观看端只看到没声音过来
// 编码格式：
// - 分享端 → 观看端 {"type": "encoders", "hardware"}：本机用显卡编得了的编码格式（(1u << rcs_codec) 的位）。
//   SDK 只告诉观看端对方编得了哪几种（rcs_peer_info.codecs），不说是显卡还是 CPU；观看端靠它只列两边都用显卡的。
//   连进来时、助手换了时发
// 低延迟模式（SDK 的 RCS_PRESET_LOW_LATENCY，两边都换）：
// - 观看端 → 分享端 {"type": "lowLatency", "on"}：连上时、改了时发。画面是大家共用的，分享端听主观看端的
// 显示器的缩略图（换显示器之前先看一眼）：
// - 观看端 → 分享端 {"type": "thumbnails"}：要每一块显示器的缩略图
// - 分享端 → 观看端 {"type": "thumbnail", "id", "width", "height", "pixels"}：一块的。pixels 是 BGRA8 的像素
//   （一行 width × 4 字节）经 qCompress 压缩再 base64；截不到的那一块不发
// 桌面的背景（设备详情顶上显示，不带窗口、图标，看不到对方在做什么）：
// - 分享端 → 观看端 {"type": "wallpaper", "jpeg"}：主显示器的背景图，JPEG 再 base64。连进来时发（界面程序读的，
//   没开着时没有），换了背景以后下一个连进来的人来时再发给连着的每一个人
// 两边是同一个版本的程序（SDK 本来就要求两端同一个版本）
namespace rc::control {

inline constexpr QLatin1StringView Displays{"displays"};
inline constexpr QLatin1StringView ShowDisplay{"showDisplay"};
inline constexpr QLatin1StringView MainViewer{"mainViewer"};
inline constexpr QLatin1StringView Sound{"sound"};
inline constexpr QLatin1StringView Encoders{"encoders"};
inline constexpr QLatin1StringView LowLatency{"lowLatency"};
inline constexpr QLatin1StringView Thumbnails{"thumbnails"};
inline constexpr QLatin1StringView Thumbnail{"thumbnail"};
inline constexpr QLatin1StringView Wallpaper{"wallpaper"};

// 分享端录不了声音的原因：没有能用的声音设备、没有录声音的授权、别的错
inline constexpr QLatin1StringView SoundNoDevice{"device"};
inline constexpr QLatin1StringView SoundNoPermission{"permission"};
inline constexpr QLatin1StringView SoundFailed{"failed"};

// 分享端的一块显示器：id 是 SDK 的 display_id；name 是系统给它的名字（显示器的型号这样的），系统不说时为空；
// 尺寸是像素；main 是主显示器
struct Display
{
    quint32 id = 0;
    QString name;
    int width = 0;
    int height = 0;
    bool main = false;

    friend bool operator==(const Display &, const Display &) = default;
};

// 分享端的显示器（按桌面上的位置从左到右），current 是正在分享的那块的 id
struct DisplayList
{
    QList<Display> displays;
    quint32 current = 0;

    friend bool operator==(const DisplayList &, const DisplayList &) = default;
};

QByteArray displays(const DisplayList &list);
QByteArray showDisplay(quint32 id);
QByteArray mainViewer(bool main);
QByteArray sound(QLatin1StringView problem);
QByteArray encoders(quint32 hardware);
QByteArray lowLatency(bool on);
QByteArray thumbnails();

// 一块显示器的缩略图：BGRA8 的像素，一行 width × 4 字节
struct DisplayPicture
{
    quint32 id = 0;
    int width = 0;
    int height = 0;
    QByteArray pixels;

    friend bool operator==(const DisplayPicture &, const DisplayPicture &) = default;
};
QByteArray thumbnail(const DisplayPicture &picture);
QByteArray wallpaper(const QByteArray &jpeg);

// 不是这种消息时 nullopt
std::optional<DisplayList> parseDisplays(const QByteArray &message);
std::optional<quint32> parseShowDisplay(const QByteArray &message);
std::optional<bool> parseMainViewer(const QByteArray &message);
std::optional<QString> parseSound(const QByteArray &message);
std::optional<quint32> parseEncoders(const QByteArray &message);
std::optional<bool> parseLowLatency(const QByteArray &message);
bool isThumbnails(const QByteArray &message);
// 像素的大小和宽高对不上时也是 nullopt
std::optional<DisplayPicture> parseThumbnail(const QByteArray &message);
// 空的也是 nullopt
std::optional<QByteArray> parseWallpaper(const QByteArray &message);

} // namespace rc::control
