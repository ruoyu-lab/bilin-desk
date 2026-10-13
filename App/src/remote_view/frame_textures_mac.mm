#include "frame_textures.h"

#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>

namespace {

struct MetalTextures final : FrameTextures
{
    // 先放掉 QRhi 的纹理，再放掉它们包着的 Metal 纹理
    ~MetalTextures() override
    {
        planes.clear();
        for (CVMetalTextureRef texture : metal)
            CFRelease(texture);
    }

    std::vector<CVMetalTextureRef> metal;
};

class MetalImporter final : public FrameTextureImporter
{
public:
    explicit MetalImporter(QRhi *rhi)
        : rhi_(rhi)
    {
        const auto *handles = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
        // Qt 把设备声明成 MTLDevice*，Metal 用的是 id<MTLDevice>
        id<MTLDevice> device = (__bridge id<MTLDevice>)(__bridge void *)handles->dev;
        CVMetalTextureCacheCreate(nullptr, nullptr, device, nullptr, &cache_);
    }

    ~MetalImporter() override { CFRelease(cache_); }

    std::unique_ptr<FrameTextures> import(const rcsdk::GpuFrame &frame) override
    {
        auto buffer = static_cast<CVPixelBufferRef>(frame.info().resource);
        auto textures = std::make_unique<MetalTextures>();
        textures->frame = frame;
        if (!addPlane(*textures, buffer, 0, MTLPixelFormatR8Unorm, QRhiTexture::R8)
            || !addPlane(*textures, buffer, 1, MTLPixelFormatRG8Unorm, QRhiTexture::RG8))
            return nullptr;
        return textures;
    }

private:
    bool addPlane(MetalTextures &textures, CVPixelBufferRef buffer, size_t index, MTLPixelFormat metalFormat,
                  QRhiTexture::Format format)
    {
        const size_t width = CVPixelBufferGetWidthOfPlane(buffer, index);
        const size_t height = CVPixelBufferGetHeightOfPlane(buffer, index);
        CVMetalTextureRef metal = nullptr;
        if (CVMetalTextureCacheCreateTextureFromImage(nullptr, cache_, buffer, nullptr, metalFormat, width, height,
                                                      index, &metal)
            != kCVReturnSuccess)
            return false;
        textures.metal.push_back(metal);
        id<MTLTexture> texture = CVMetalTextureGetTexture(metal);
        std::unique_ptr<QRhiTexture> plane(rhi_->newTexture(format, QSize(int(width), int(height))));
        if (!plane->createFrom({quint64(quintptr((__bridge void *)texture)), 0}))
            return false;
        textures.planes.push_back(std::move(plane));
        return true;
    }

    QRhi *rhi_;
    CVMetalTextureCacheRef cache_ = nullptr;
};

} // namespace

std::unique_ptr<FrameTextureImporter> FrameTextureImporter::create(QRhi *rhi)
{
    return std::make_unique<MetalImporter>(rhi);
}
