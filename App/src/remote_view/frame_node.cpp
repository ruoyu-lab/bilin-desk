#include "frame_node.h"

#include "frame_mailbox.h"

#include <QFile>
#include <QQuickWindow>
#include <rhi/qshader.h>

namespace {

// 画面在 GPU 还在读的时候不能还给解码器：留住正在画的和之前的两帧
constexpr size_t KeptFrames = 3;

QShader loadShader(const QString &name)
{
    // 着色器编进了程序（qt_add_shaders），打不开就是构建出了错
    QFile file(QStringLiteral(":/shaders/%1.qsb").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("程序里没有着色器 %s", qUtf8Printable(name));
    return QShader::fromSerialized(file.readAll());
}

} // namespace

FrameNode::FrameNode(QQuickWindow *window)
    : window_(window)
{
}

FrameNode::~FrameNode()
{
    releaseResources();
}

void FrameNode::update(const rcsdk::GpuFrame &frame, const QRectF &target, const QRectF &source,
                       std::shared_ptr<FrameMailbox> frames)
{
    if (frame.handle() != pending_.handle()) {
        pending_ = frame;
        fresh_ = true;
    }
    frames_ = std::move(frames);
    target_ = target;
    source_ = source;
    markDirty(QSGNode::DirtyMaterial);
}

void FrameNode::importPending()
{
    if (const rcsdk::GpuFrame newer = frames_->newerThan(pending_)) {
        pending_ = newer;
        fresh_ = true;
    }
    if (!fresh_)
        return;
    fresh_ = false;
    std::unique_ptr<FrameTextures> textures = importer_->import(pending_);
    if (!textures) {
        // 接着画上一帧；同一个原因只记一次
        if (!importFailed_)
            qWarning("远程画面的一帧做不成纹理（显存不够或显卡设备丢了），先不画新的帧");
        importFailed_ = true;
        return;
    }
    importFailed_ = false;
    textures_.push_back(std::move(textures));
    while (textures_.size() > KeptFrames)
        textures_.pop_front();
    bindings_.reset();
}

void FrameNode::prepare()
{
    QRhi *rhi = window_->rhi();
    QRhiRenderTarget *target = renderTarget();
    if (!importer_)
        importer_ = FrameTextureImporter::create(rhi);
    importPending();
    if (textures_.empty())
        return;

    const FrameTextures &current = *textures_.back();
    const rcs_gpu_frame_info info = current.frame.info();
    if (!vertices_) {
        vertices_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(float) * 16));
        vertices_->create();
        uniforms_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 80));
        uniforms_->create();
        sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                       QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        sampler_->create();
    }
    if (!bindings_) {
        const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
        QList<QRhiShaderResourceBinding> list{QRhiShaderResourceBinding::uniformBuffer(0, stages, uniforms_.get())};
        for (size_t i = 0; i < current.planes.size(); ++i)
            list.append(QRhiShaderResourceBinding::sampledTexture(int(i) + 1, QRhiShaderResourceBinding::FragmentStage,
                                                                  current.planes[i].get(), sampler_.get()));
        bindings_.reset(rhi->newShaderResourceBindings());
        bindings_->setBindings(list.cbegin(), list.cend());
        bindings_->create();
        // 格式没变时布局一样，管线照用
        if (pipeline_ && pipelineFormat_ == info.format)
            pipeline_->setShaderResourceBindings(bindings_.get());
    }
    if (!pipeline_ || pipelinePass_ != target->renderPassDescriptor() || pipelineFormat_ != info.format)
        createPipeline(rhi, target, info.format);

    const auto x0 = float(target_.left()), y0 = float(target_.top());
    const auto x1 = float(target_.right()), y1 = float(target_.bottom());
    const auto u0 = float(source_.left()), v0 = float(source_.top());
    const auto u1 = float(source_.right()), v1 = float(source_.bottom());
    const float quad[16] = {x0, y0, u0, v0, x1, y0, u1, v0, x0, y1, u0, v1, x1, y1, u1, v1};
    const QMatrix4x4 mvp = *projectionMatrix() * *matrix();
    const float parameters[2] = {info.color.matrix == RCS_MATRIX_BT601 ? 0.0f : 1.0f, float(inheritedOpacity())};
    QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(vertices_.get(), 0, sizeof(quad), quad);
    batch->updateDynamicBuffer(uniforms_.get(), 0, 64, mvp.constData());
    batch->updateDynamicBuffer(uniforms_.get(), 64, sizeof(parameters), parameters);
    commandBuffer()->resourceUpdate(batch);
}

void FrameNode::render(const RenderState *state)
{
    if (textures_.empty())
        return;
    QRhiCommandBuffer *commands = commandBuffer();
    const QSize size = renderTarget()->pixelSize();
    commands->setGraphicsPipeline(pipeline_.get());
    commands->setViewport(QRhiViewport(0, 0, float(size.width()), float(size.height())));
    const QRect scissor = state->scissorEnabled() ? state->scissorRect() : QRect(QPoint(), size);
    commands->setScissor(QRhiScissor(scissor.x(), scissor.y(), scissor.width(), scissor.height()));
    commands->setShaderResources(bindings_.get());
    const QRhiCommandBuffer::VertexInput input(vertices_.get(), 0);
    commands->setVertexInput(0, 1, &input);
    commands->draw(4);
}

void FrameNode::releaseResources()
{
    pipeline_.reset();
    bindings_.reset();
    sampler_.reset();
    uniforms_.reset();
    vertices_.reset();
    textures_.clear();
    importer_.reset();
}

QSGRenderNode::StateFlags FrameNode::changedStates() const
{
    return ViewportState | ScissorState;
}

QSGRenderNode::RenderingFlags FrameNode::flags() const
{
    return BoundedRectRendering;
}

QRectF FrameNode::rect() const
{
    return target_;
}

void FrameNode::createPipeline(QRhi *rhi, QRhiRenderTarget *target, rcs_pixel_format format)
{
    pipeline_.reset(rhi->newGraphicsPipeline());
    pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline_->setFlags(QRhiGraphicsPipeline::UsesScissor);
    pipeline_->setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(QStringLiteral("frame.vert"))},
        {QRhiShaderStage::Fragment,
         loadShader(format == RCS_PIXEL_BGRA8 ? QStringLiteral("bgra.frag") : QStringLiteral("nv12.frag"))},
    });
    QRhiVertexInputLayout layout;
    layout.setBindings({{4 * sizeof(float)}});
    layout.setAttributes(
        {{0, 0, QRhiVertexInputAttribute::Float2, 0}, {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}});
    pipeline_->setVertexInputLayout(layout);
    // 预乘透明度：条目的不透明度也算进去
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    pipeline_->setTargetBlends({blend});
    pipeline_->setSampleCount(target->sampleCount());
    pipeline_->setShaderResourceBindings(bindings_.get());
    pipeline_->setRenderPassDescriptor(target->renderPassDescriptor());
    pipeline_->create();
    pipelinePass_ = target->renderPassDescriptor();
    pipelineFormat_ = format;
}
