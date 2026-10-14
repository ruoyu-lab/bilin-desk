#pragma once

#include <rcsdk/rcsdk.hpp>

#include <rhi/qrhi.h>

#include <memory>
#include <vector>

// 解出的一帧 GPU 画面包成 QRhi 纹理，不复制。
// NV12（硬件解码；macOS 的软件解码也是）是两个平面：亮度 R8、色度 RG8。
// macOS 上是 CVPixelBuffer 的两个平面经 CVMetalTextureCache 做成的 Metal 纹理（frame_textures_mac.mm）；
// Windows 上是 D3D11 纹理的着色器视图，软件解码时是一张 BGRA8 纹理（frame_textures_win.cpp）；
// Android 上是解码器的硬件缓冲区做成的 OpenGL ES 外部纹理，GPU 采样时自己转成 RGB（frame_textures_android.cpp）。
// 帧在会话窗口画画用的设备上（ViewGraphics）
struct FrameTextures
{
    virtual ~FrameTextures() = default;

    rcsdk::GpuFrame frame; // 留住这一帧：GPU 可能还在读它
    // NV12：亮度、色度；BGRA8、外部纹理：一张
    std::vector<std::unique_ptr<QRhiTexture>> planes;
    // 画它的片段着色器（shaders/ 里的名字）：nv12 | bgra | external
    QString shader;
};

class FrameTextureImporter
{
public:
    virtual ~FrameTextureImporter() = default;

    // 系统给不出纹理时（显存不够、设备丢了）为空
    virtual std::unique_ptr<FrameTextures> import(const rcsdk::GpuFrame &frame) = 0;

    static std::unique_ptr<FrameTextureImporter> create(QRhi *rhi);
};
