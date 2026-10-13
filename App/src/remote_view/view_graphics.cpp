#include "view_graphics.h"

#include <QJSEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>

#ifdef Q_OS_WIN
#include <QQuickGraphicsDevice>
// ID3D10Multithread：较新的 Windows SDK 要求 d3d10_1.h 在 d3d10.h 之前
#include <d3d10_1.h>
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct ViewGraphics::Device
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D10Multithread> lock;
};
#endif

namespace {

ViewGraphics *instance = nullptr;

} // namespace

ViewGraphics::ViewGraphics(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    auto device = std::make_unique<Device>();
    const HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                             D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                             nullptr, 0, D3D11_SDK_VERSION, &device->device, nullptr, &device->context);
    if (FAILED(result)) {
        qWarning("显卡建不起解码远程画面用的 D3D11 设备（0x%08lx），远程画面改用 CPU 上的", static_cast<unsigned long>(result));
        return;
    }
    device->context.As(&device->lock);
    device->lock->SetMultithreadProtected(TRUE);
    device_ = std::move(device);
#elif defined(Q_OS_APPLE)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    // Android：OpenGL ES
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
}

ViewGraphics::~ViewGraphics() = default;

void ViewGraphics::setInstance(ViewGraphics *graphics)
{
    instance = graphics;
}

ViewGraphics *ViewGraphics::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void *ViewGraphics::decodeDevice() const
{
#ifdef Q_OS_WIN
    return device_ ? device_->device.Get() : nullptr;
#else
    return nullptr;
#endif
}

void ViewGraphics::prepare(QQuickWindow *window)
{
    QSurfaceFormat format = window->requestedFormat();
    format.setSwapInterval(0);
    window->setFormat(format);
#ifdef Q_OS_WIN
    if (!device_)
        return;
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndContext(device_->device.Get(), device_->context.Get()));
    // 一帧从开始准备到提交完都持有上下文的锁：别的窗口、SDK 的解码线程这时不会插进来改它的状态
    ID3D10Multithread *lock = device_->lock.Get();
    connect(window, &QQuickWindow::beforeFrameBegin, window, [lock] { lock->Enter(); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterFrameEnd, window, [lock] { lock->Leave(); }, Qt::DirectConnection);
#endif
}
