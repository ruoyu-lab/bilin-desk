#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;

// 会话窗口画远程画面用的图形设置（QML 单例，main 创建）。
// - 图形接口：Windows 用 D3D11，macOS、iOS 用 Metal，Android 用 OpenGL ES
// - 画完立即交给系统合成，不等显示器刷新：等的话流的帧率和刷新率相同时会排上两三帧，延迟翻倍
// - Windows：建一个 D3D11 设备（带视频接口和多线程保护），会话窗口都用它画、SDK 也在它上面解码，
//   解出的纹理不用复制就能画。几个会话窗口的渲染线程和 SDK 的解码线程共用它的即时上下文，
//   每个窗口画一帧的前后锁住它。显卡建不起这样的设备时（没装显卡驱动、虚拟机、远程桌面里）
//   decodeDevice() 为空，会话改用 CPU 上的画面（ViewerSession）
class ViewGraphics : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。要在任何窗口之前创建
    explicit ViewGraphics(QObject *parent);
    ~ViewGraphics() override;

    static void setInstance(ViewGraphics *instance);
    static ViewGraphics *create(QQmlEngine *, QJSEngine *);

    // Windows：SDK 解码用的 ID3D11Device*；建不起来时、其他系统上为空
    void *decodeDevice() const;
    // 会话窗口显示之前调用
    Q_INVOKABLE void prepare(QQuickWindow *window);

private:
#ifdef Q_OS_WIN
    struct Device;
    std::unique_ptr<Device> device_;
#endif
};
