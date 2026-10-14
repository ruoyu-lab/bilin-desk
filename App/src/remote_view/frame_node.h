#pragma once

#include "frame_textures.h"

#include <rcsdk/rcsdk.hpp>

#include <QRectF>
#include <QSGRenderNode>

#include <deque>
#include <memory>

class FrameMailbox;
class QQuickWindow;

// 把观看会话解出的 GPU 帧直接画进 Qt Quick 的渲染目标（QRhi：macOS 上 Metal，Windows 上 D3D11）：
// 帧的纹理（FrameTextures）用自己的着色器转成 RGB，画成一个矩形，不复制、没有中间纹理。
// 两次同步之间，信箱里形状一样的新帧由渲染线程直接画（FrameMailbox）
class FrameNode final : public QSGRenderNode
{
public:
    explicit FrameNode(QQuickWindow *window);
    ~FrameNode() override;

    // 同步时（界面线程停着）：画哪一帧、画在条目坐标的哪里、取帧表面的哪一块（0..1）
    void update(const rcsdk::GpuFrame &frame, const QRectF &target, const QRectF &source,
                std::shared_ptr<FrameMailbox> frames);

    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;

private:
    void importPending();
    void createPipeline(QRhi *rhi, QRhiRenderTarget *target, const QString &shader);

    QQuickWindow *window_;
    std::shared_ptr<FrameMailbox> frames_;
    rcsdk::GpuFrame pending_; // 要画的那一帧
    bool fresh_ = false;      // pending_ 还没做成纹理
    bool importFailed_ = false;
    QRectF target_;
    QRectF source_;

    std::unique_ptr<FrameTextureImporter> importer_;
    // 正在画的在最后；前面几帧 GPU 可能还在读，留着
    std::deque<std::unique_ptr<FrameTextures>> textures_;
    std::unique_ptr<QRhiBuffer> vertices_;
    std::unique_ptr<QRhiBuffer> uniforms_;
    std::unique_ptr<QRhiSampler> sampler_;
    std::unique_ptr<QRhiShaderResourceBindings> bindings_;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline_;
    QRhiRenderPassDescriptor *pipelinePass_ = nullptr;
    QString pipelineShader_;
};
