#include "frame_mailbox.h"

#include <QQuickWindow>
#include <QRunnable>

namespace {

bool sameShape(const rcs_gpu_frame_info &a, const rcs_gpu_frame_info &b)
{
    return a.width == b.width && a.height == b.height && a.surface_width == b.surface_width
           && a.surface_height == b.surface_height && a.format == b.format;
}

} // namespace

bool FrameMailbox::putFrame(const rcsdk::GpuFrame &frame)
{
    bool same = false;
    {
        std::lock_guard lock(frameMutex_);
        same = frame_ && sameShape(frame_.info(), frame.info());
        frame_ = frame;
    }
    if (!same)
        return false;
    std::lock_guard lock(windowMutex_);
    if (!window_)
        return false;
    window_->scheduleRenderJob(QRunnable::create([window = window_] { window->update(); }), QQuickWindow::NoStage);
    return true;
}

void FrameMailbox::putImage(const QImage &image)
{
    std::lock_guard lock(frameMutex_);
    image_ = image;
}

rcsdk::GpuFrame FrameMailbox::latestFrame() const
{
    std::lock_guard lock(frameMutex_);
    return frame_;
}

QImage FrameMailbox::latestImage() const
{
    std::lock_guard lock(frameMutex_);
    return image_;
}

rcsdk::GpuFrame FrameMailbox::newerThan(const rcsdk::GpuFrame &drawn) const
{
    const rcsdk::GpuFrame newest = latestFrame();
    if (!newest || !drawn || newest.handle() == drawn.handle())
        return {};
    const rcs_gpu_frame_info a = newest.info();
    const rcs_gpu_frame_info b = drawn.info();
    return sameShape(a, b) && a.sequence > b.sequence ? newest : rcsdk::GpuFrame();
}

void FrameMailbox::setRenderWindow(QQuickWindow *window)
{
    std::lock_guard lock(windowMutex_);
    window_ = window;
}

void FrameMailbox::clear()
{
    std::lock_guard lock(frameMutex_);
    frame_ = {};
    image_ = {};
}
