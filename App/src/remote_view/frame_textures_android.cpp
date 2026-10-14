#include "frame_textures.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>

namespace {

// 解码器的硬件缓冲区（AHardwareBuffer）做成 EGLImage，再做成 OpenGL ES 的外部纹理（GL_TEXTURE_EXTERNAL_OES）：
// 不复制，采样时 GPU 按缓冲区标的格式转成 RGB（shaders/external_gles.frag）
struct ExternalTextures final : FrameTextures
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint texture = 0;
    PFNEGLDESTROYIMAGEKHRPROC destroyImage = nullptr;

    // 在渲染线程上、上下文是当前的时候（FrameNode 丢掉它的时候）
    ~ExternalTextures() override
    {
        planes.clear();
        if (texture)
            glDeleteTextures(1, &texture);
        if (image != EGL_NO_IMAGE_KHR)
            destroyImage(display, image);
    }
};

class ExternalImporter final : public FrameTextureImporter
{
public:
    explicit ExternalImporter(QRhi *rhi)
        : rhi_(rhi)
        , getClientBuffer_(reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
              eglGetProcAddress("eglGetNativeClientBufferANDROID")))
        , createImage_(reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR")))
        , destroyImage_(reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR")))
        , targetTexture_(reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
              eglGetProcAddress("glEGLImageTargetTexture2DOES")))
    {
        // Android 10 起的 OpenGL ES 都有这几个（SDK 也只在这样的手机上交硬件缓冲区）
        Q_ASSERT(getClientBuffer_ && createImage_ && destroyImage_ && targetTexture_);
    }

    std::unique_ptr<FrameTextures> import(const rcsdk::GpuFrame &frame) override
    {
        const rcs_gpu_frame_info info = frame.info();
        rhi_->makeThreadLocalNativeContextCurrent();
        auto textures = std::make_unique<ExternalTextures>();
        textures->frame = frame;
        textures->shader = QStringLiteral("external");
        textures->display = eglGetCurrentDisplay();
        textures->destroyImage = destroyImage_;
        const EGLint attributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
        textures->image = createImage_(textures->display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                       getClientBuffer_(static_cast<AHardwareBuffer *>(info.resource)), attributes);
        if (textures->image == EGL_NO_IMAGE_KHR)
            return nullptr;
        glGenTextures(1, &textures->texture);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, textures->texture);
        targetTexture_(GL_TEXTURE_EXTERNAL_OES, static_cast<GLeglImageOES>(textures->image));
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        std::unique_ptr<QRhiTexture> plane(rhi_->newTexture(
            QRhiTexture::RGBA8, QSize(int(info.surface_width), int(info.surface_height)), 1, QRhiTexture::ExternalOES));
        if (!plane->createFrom({textures->texture, 0}))
            return nullptr;
        textures->planes.push_back(std::move(plane));
        return textures;
    }

private:
    QRhi *rhi_;
    PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC getClientBuffer_;
    PFNEGLCREATEIMAGEKHRPROC createImage_;
    PFNEGLDESTROYIMAGEKHRPROC destroyImage_;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC targetTexture_;
};

} // namespace

std::unique_ptr<FrameTextureImporter> FrameTextureImporter::create(QRhi *rhi)
{
    return std::make_unique<ExternalImporter>(rhi);
}
