#pragma once

#include <rcsdk/rcsdk.hpp>

#include <QImage>

#include <mutex>

class QQuickWindow;

// 观看会话解出的最新一帧：SDK 的线程放进来，渲染线程取走去画。会话和画它的渲染节点共有它
// （std::shared_ptr），谁先没了都不要紧。
// 一般是 GPU 上的帧：和上一帧形状一样（尺寸、格式）、又有窗口在画时，新的一帧直接交给那个窗口的
// 渲染线程，不经界面线程和同步，延迟最短；第一帧和形状变了的帧要经界面线程重新布局。
// 显卡建不起解码设备时（ViewGraphics）是 CPU 上的图像，每一帧都经界面线程
class FrameMailbox
{
public:
    // SDK 的线程：放进新的一帧。true：已经交给了渲染线程；false：要经界面线程通知画面
    bool putFrame(const rcsdk::GpuFrame &frame);
    void putImage(const QImage &image);
    // 最新的一帧；没有时为空
    rcsdk::GpuFrame latestFrame() const;
    QImage latestImage() const;
    // 渲染线程：比 drawn 新、形状一样的一帧；没有时为空
    rcsdk::GpuFrame newerThan(const rcsdk::GpuFrame &drawn) const;
    // 正在画这些帧的窗口；nullptr：没有，新的帧都经界面线程
    void setRenderWindow(QQuickWindow *window);
    void clear();

private:
    mutable std::mutex frameMutex_;
    rcsdk::GpuFrame frame_;
    QImage image_;
    std::mutex windowMutex_;
    QQuickWindow *window_ = nullptr;
};
