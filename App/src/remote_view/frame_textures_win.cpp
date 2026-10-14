#include "frame_textures.h"

namespace {

class D3D11Importer final : public FrameTextureImporter
{
public:
    explicit D3D11Importer(QRhi *rhi)
        : rhi_(rhi)
    {
    }

    // NV12 纹理的两个着色器视图：R8 对着亮度平面，RG8 对着色度平面（D3D11 按视图的格式选平面）
    std::unique_ptr<FrameTextures> import(const rcsdk::GpuFrame &frame) override
    {
        const rcs_gpu_frame_info info = frame.info();
        const QSize size(int(info.surface_width), int(info.surface_height));
        const QRhiTexture::NativeTexture native{quint64(quintptr(info.resource)), 0};
        auto textures = std::make_unique<FrameTextures>();
        textures->frame = frame;
        textures->shader = info.format == RCS_PIXEL_BGRA8 ? QStringLiteral("bgra") : QStringLiteral("nv12");
        if (info.format == RCS_PIXEL_BGRA8) {
            if (!addPlane(*textures, QRhiTexture::BGRA8, size, native))
                return nullptr;
        } else if (!addPlane(*textures, QRhiTexture::R8, size, native)
                   || !addPlane(*textures, QRhiTexture::RG8, size / 2, native)) {
            return nullptr;
        }
        return textures;
    }

private:
    bool addPlane(FrameTextures &textures, QRhiTexture::Format format, QSize size,
                  const QRhiTexture::NativeTexture &native)
    {
        std::unique_ptr<QRhiTexture> plane(rhi_->newTexture(format, size));
        if (!plane->createFrom(native))
            return false;
        textures.planes.push_back(std::move(plane));
        return true;
    }

    QRhi *rhi_;
};

} // namespace

std::unique_ptr<FrameTextureImporter> FrameTextureImporter::create(QRhi *rhi)
{
    return std::make_unique<D3D11Importer>(rhi);
}
