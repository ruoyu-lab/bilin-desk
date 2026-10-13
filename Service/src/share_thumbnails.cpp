#include "share_thumbnails.h"

#include <rcsdk/rcsdk.hpp>
#include <rcsdk/thumbnail.h>

#include <cstring>
#include <optional>
#include <utility>

namespace {

// 观看端的弹出框里一块两百来个点宽，Retina 上两倍
constexpr uint32_t MaxWidth = 400;
constexpr uint32_t MaxHeight = 250;

// 截一块：不成（显示器刚拔掉、助手不在）时为空
std::optional<rc::control::DisplayPicture> take(rcs_session session, quint32 display)
{
    rcs_capture_config what = rcs_capture_config_init();
    what.source = RCS_CAPTURE_DISPLAY;
    what.display_id = display;
    QByteArray buffer(qsizetype(MaxWidth) * MaxHeight * 4, Qt::Uninitialized);
    rcs_picture picture = rcs_picture_init();
    picture.data = reinterpret_cast<uint8_t *>(buffer.data());
    picture.capacity = uint64_t(buffer.size());
    if (const rcs_result result = rcs_thumbnail_capture(session, &what, MaxWidth, MaxHeight, &picture);
        result != RCS_OK) {
        qInfo("截不到显示器 %u 的缩略图：%s", display, rcs_result_message(result));
        return std::nullopt;
    }
    // 一行一行挤紧（截出来的一行后面可能有空）
    rc::control::DisplayPicture thumbnail{display, int(picture.width), int(picture.height), {}};
    const qsizetype row = qsizetype(picture.width) * 4;
    thumbnail.pixels.resize(row * picture.height);
    for (uint32_t y = 0; y < picture.height; ++y)
        std::memcpy(thumbnail.pixels.data() + y * row, picture.data + qsizetype(y) * picture.stride_bytes, size_t(row));
    return thumbnail;
}

} // namespace

ShareThumbnails::ShareThumbnails(std::function<void(int, const QByteArray &)> send, QObject *parent)
    : QObject(parent)
    , send_(std::move(send))
{
}

ShareThumbnails::~ShareThumbnails()
{
    stop();
}

void ShareThumbnails::configure(rcsdk::SessionConfig &config)
{
    config.useModule(RCS_THUMBNAIL_MODULE, nullptr, 0);
}

void ShareThumbnails::request(int viewer, rcs_session session, const QList<quint32> &displays)
{
    if (worker_.joinable()) {
        waiting_.insert(viewer);
        return;
    }
    taking_ = {viewer};
    start(session, displays);
}

void ShareThumbnails::stop()
{
    ++round_;
    if (worker_.joinable())
        worker_.join();
    taking_.clear();
    waiting_.clear();
}

void ShareThumbnails::start(rcs_session session, const QList<quint32> &displays)
{
    session_ = session;
    displays_ = displays;
    const quint64 round = ++round_;
    worker_ = std::thread([this, round, session, displays] {
        QList<rc::control::DisplayPicture> pictures;
        for (const quint32 display : displays) {
            if (std::optional<rc::control::DisplayPicture> picture = take(session, display))
                pictures.append(std::move(*picture));
        }
        // stop()、析构都先等本线程结束，这时对象还在
        QMetaObject::invokeMethod(this, [this, round, pictures] { finished(round, pictures); }, Qt::QueuedConnection);
    });
}

void ShareThumbnails::finished(quint64 round, const QList<rc::control::DisplayPicture> &pictures)
{
    if (round != round_)
        return;
    worker_.join();
    for (const rc::control::DisplayPicture &picture : pictures) {
        const QByteArray message = rc::control::thumbnail(picture);
        for (const int viewer : std::as_const(taking_))
            send_(viewer, message);
    }
    taking_ = std::exchange(waiting_, {});
    if (!taking_.isEmpty())
        start(session_, displays_);
}
