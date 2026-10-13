#include "remote_view.h"

#include "frame_mailbox.h"
#ifdef RC_GPU_FRAMES
#include "frame_node.h"
#endif
#include "viewer_session.h"

#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QStyleHints>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <span>

namespace {

// 本机指针换成了"对方的指针看不见"
constexpr uint32_t HiddenCursor = std::numeric_limits<uint32_t>::max();

// 只看时画在画面上的对方指针：一张图，缩放到条目的大小
class CursorOverlay final : public QQuickPaintedItem
{
public:
    using QQuickPaintedItem::QQuickPaintedItem;

    void show(const QImage &image)
    {
        if (image.cacheKey() == image_.cacheKey())
            return;
        image_ = image;
        update();
    }

    void paint(QPainter *painter) override
    {
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(boundingRect(), image_);
    }

private:
    QImage image_;
};

} // namespace

RemoteView::RemoteView(QQuickItem *parent)
    : QQuickItem(parent)
    , cursorOverlay_(new CursorOverlay(this))
{
    cursorOverlay_->setVisible(false);
    cursorOverlay_->setZ(1);
    setFlag(ItemHasContents);
    setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton | Qt::BackButton
                            | Qt::ForwardButton);
    connect(this, &RemoteView::hasFrameChanged, this, &RemoteView::updateCursor);
    connect(this, &QQuickItem::widthChanged, this, &RemoteView::updateCursor);
    connect(this, &QQuickItem::heightChanged, this, &RemoteView::updateCursor);
}

RemoteView::~RemoteView()
{
    if (frames_)
        frames_->setRenderWindow(nullptr);
}

void RemoteView::setSession(ViewerSession *session)
{
    if (session == session_)
        return;
    if (session_) {
        disconnect(session_, nullptr, this, nullptr);
        frames_->setRenderWindow(nullptr);
    }
    session_ = session;
    frames_ = session ? session->frames() : nullptr;
    if (session) {
        connect(session, &ViewerSession::frameReady, this, &QQuickItem::update);
        connect(session, &ViewerSession::streamChanged, this, &RemoteView::updateCursor);
        connect(session, &ViewerSession::cursorChanged, this, &RemoteView::updateCursor);
        // 连上以后才知道对方的系统
        connect(session, &ViewerSession::stateChanged, this, &RemoteView::shortcutsChanged);
    }
    emit sessionChanged();
    emit shortcutsChanged();
    update();
}

void RemoteView::setNativeSize(bool nativeSize)
{
    if (nativeSize == nativeSize_)
        return;
    nativeSize_ = nativeSize;
    panX_ = 0;
    panY_ = 0;
    emit nativeSizeChanged();
    updateCursor();
    update();
}

void RemoteView::setInputEnabled(bool enabled)
{
    if (enabled == inputEnabled_)
        return;
    if (inputEnabled_)
        releaseAll();
    inputEnabled_ = enabled;
    setAcceptHoverEvents(enabled);
    emit inputEnabledChanged();
    updateCursor();
}

void RemoteView::setRemoteCursor(bool remoteCursor)
{
    if (remoteCursor == remoteCursor_)
        return;
    remoteCursor_ = remoteCursor;
    emit remoteCursorChanged();
    updateCursor();
}

QVariantList RemoteView::shortcuts() const
{
    QVariantList list;
    // 对方执行得了 Ctrl+Alt+Del（Windows 装好了系统服务）时排第一个：锁屏、登录界面上要先按它
    if (session_ && (session_->peerInputTypes() & (1u << RCS_INPUT_SECURE_ATTENTION))) {
        list.append(QVariantMap{
            {QStringLiteral("id"), QString::fromLatin1(remote_input::SecureAttention)},
            {QStringLiteral("label"), tr("安全选项（锁屏、登录）")},
            {QStringLiteral("keys"), QStringLiteral("Ctrl+Alt+Del")},
        });
    }
    for (const remote_input::Shortcut &shortcut : remote_input::shortcuts(remoteIsMac())) {
        list.append(QVariantMap{
            {QStringLiteral("id"), QString::fromLatin1(shortcut.id)},
            {QStringLiteral("label"), QCoreApplication::translate("RemoteView", shortcut.label)},
            {QStringLiteral("keys"), QString::fromUtf8(shortcut.keys)},
        });
    }
    return list;
}

void RemoteView::sendShortcut(const QString &id)
{
    if (id == QLatin1StringView(remote_input::SecureAttention)) {
        rcs_input_event event = rcs_input_event_init();
        event.type = RCS_INPUT_SECURE_ATTENTION;
        send(event);
        return;
    }
    const auto shortcuts = remote_input::shortcuts(remoteIsMac());
    const auto shortcut = std::find_if(shortcuts.begin(), shortcuts.end(),
                                       [&id](const remote_input::Shortcut &s) { return id == QLatin1StringView(s.id); });
    // 用不满的后面是 0
    const std::span<const uint32_t> keys(std::begin(shortcut->usages), std::ranges::find(shortcut->usages, 0u));
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_KEY;
    event.down = 1;
    for (const uint32_t key : keys) {
        event.code = key;
        send(event);
    }
    event.down = 0;
    for (auto key = keys.rbegin(); key != keys.rend(); ++key) {
        event.code = *key;
        send(event);
    }
}

bool RemoteView::remoteIsMac() const
{
    return session_ && session_->peerPlatform() == RCS_PLATFORM_MACOS;
}

rcsdk::ViewGeometry RemoteView::geometry() const
{
    const qreal ratio = window()->effectiveDevicePixelRatio();
    return rcsdk::viewGeometry(int(width() * ratio), int(height() * ratio), frameWidth_, frameHeight_,
                               nativeSize_ ? rcsdk::ViewScale::nativePixels : rcsdk::ViewScale::fit, panX_, panY_);
}

QSGNode *RemoteView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    const rcsdk::GpuFrame frame = frames_ ? frames_->latestFrame() : rcsdk::GpuFrame();
    const QImage image = frames_ && !frame ? frames_->latestImage() : QImage();
    const NodeKind kind = frame ? NodeKind::Frame : image.isNull() ? NodeKind::None : NodeKind::Image;
    if (kind != nodeKind_) {
        delete old;
        old = nullptr;
        nodeKind_ = kind;
        imageKey_ = 0;
    }
    // GPU 上形状一样的新帧由渲染线程直接画（FrameMailbox）
    if (frames_)
        frames_->setRenderWindow(kind == NodeKind::Frame ? window() : nullptr);
    if (kind == NodeKind::None) {
        frameWidth_ = 0;
        frameHeight_ = 0;
        publishHasFrame(false);
        return nullptr;
    }

    const qreal ratio = window()->effectiveDevicePixelRatio();
    QSGNode *node = old;
    if (kind == NodeKind::Frame) {
#ifdef RC_GPU_FRAMES
        const rcs_gpu_frame_info info = frame.info();
        frameWidth_ = info.width;
        frameHeight_ = info.height;
        const rcsdk::ViewGeometry g = geometry();
        // 帧的表面可能比画面大：取画面那一块（0..1）
        const QRectF source(double(g.sourceX) / info.surface_width, double(g.sourceY) / info.surface_height,
                            double(g.sourceWidth) / info.surface_width, double(g.sourceHeight) / info.surface_height);
        auto *frameNode = old ? static_cast<FrameNode *>(old) : new FrameNode(window());
        frameNode->update(frame, QRectF(g.x / ratio, g.y / ratio, g.width / ratio, g.height / ratio), source, frames_);
        node = frameNode;
#endif
    } else {
        frameWidth_ = uint32_t(image.width());
        frameHeight_ = uint32_t(image.height());
        const rcsdk::ViewGeometry g = geometry();
        auto *imageNode = static_cast<QSGSimpleTextureNode *>(old);
        if (!imageNode) {
            imageNode = new QSGSimpleTextureNode;
            imageNode->setOwnsTexture(true);
            imageNode->setFiltering(QSGTexture::Linear);
        }
        if (image.cacheKey() != imageKey_) {
            imageNode->setTexture(window()->createTextureFromImage(image));
            imageKey_ = image.cacheKey();
        }
        imageNode->setRect(QRectF(g.x / ratio, g.y / ratio, g.width / ratio, g.height / ratio));
        imageNode->setSourceRect(QRectF(g.sourceX, g.sourceY, g.sourceWidth, g.sourceHeight));
        node = imageNode;
    }
    publishHasFrame(true);
    return node;
}

void RemoteView::publishHasFrame(bool hasFrame)
{
    if (hasFrame == hasFrame_)
        return;
    hasFrame_ = hasFrame;
    QMetaObject::invokeMethod(this, &RemoteView::hasFrameChanged, Qt::QueuedConnection);
}

void RemoteView::itemChange(ItemChange change, const ItemChangeData &value)
{
    // 窗口隐藏、关掉之前，它的渲染线程不再从 SDK 的线程接活
    if (change == ItemSceneChange && value.window) {
        connect(value.window, &QQuickWindow::sceneGraphInvalidated, this, [this] {
            if (frames_)
                frames_->setRenderWindow(nullptr);
        }, Qt::DirectConnection);
    }
    // 不在前面的标签不画：新的帧也不再让渲染线程重画窗口；再显示时从最新的一帧画起
    if (change == ItemVisibleHasChanged && frames_) {
        if (value.boolValue)
            update();
        else
            frames_->setRenderWindow(nullptr);
    }
    QQuickItem::itemChange(change, value);
}

void RemoteView::updateCursor()
{
    const auto restoreLocal = [this] {
        if (!localCursorShape_)
            return;
        unsetCursor();
        localCursorShape_.reset();
    };
    const ViewerSession::CursorShape *shape =
        session_ && frameWidth_ && remoteCursor_ ? session_->cursorShape(session_->cursor().shapeId) : nullptr;
    if (!shape) {
        cursorOverlay_->setVisible(false);
        restoreLocal();
        return;
    }
    const ViewerSession::Cursor cursor = session_->cursor();
    const rcs_stream_info info = session_->streamInfo();
    const qreal ratio = window()->effectiveDevicePixelRatio();
    const rcsdk::ViewGeometry g = geometry();
    const double scale = rcsdk::displayScale(g, info) / ratio; // 对方显示器的一个像素在条目里多大
    const QSizeF size(shape->image.width() * scale, shape->image.height() * scale);

    // 控制时：本机的指针换成对方指针的样子
    if (inputEnabled_) {
        cursorOverlay_->setVisible(false);
        if (!cursor.visible) {
            setCursor(Qt::BlankCursor);
            localCursorShape_ = HiddenCursor;
        } else if (localCursorShape_ != cursor.shapeId || localCursorScale_ != scale) {
            QPixmap pixmap = QPixmap::fromImage(
                shape->image.scaled((size * ratio).toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            pixmap.setDevicePixelRatio(ratio);
            setCursor(QCursor(pixmap, int(shape->hotspot.x() * scale), int(shape->hotspot.y() * scale)));
            localCursorShape_ = cursor.shapeId;
            localCursorScale_ = scale;
        }
        return;
    }

    // 只看时：在画面上对方指针的位置画出来
    restoreLocal();
    uint32_t x = cursor.x;
    uint32_t y = cursor.y;
    rcsdk::framePosition(x, y, info);
    double px = 0;
    double py = 0;
    if (!cursor.visible || !g.point(x, y, frameWidth_, frameHeight_, px, py)) {
        cursorOverlay_->setVisible(false);
        return;
    }
    static_cast<CursorOverlay *>(cursorOverlay_)->show(shape->image);
    cursorOverlay_->setSize(size);
    cursorOverlay_->setPosition(
        QPointF(px / ratio - shape->hotspot.x() * scale, py / ratio - shape->hotspot.y() * scale));
    cursorOverlay_->setVisible(true);
}

bool RemoteView::position(QPointF point, rcs_input_event &event, bool clamp) const
{
    if (!frameWidth_)
        return false;
    const qreal ratio = window()->effectiveDevicePixelRatio();
    // 条目坐标 → 编码帧 → 对方的显示器（去掉编码时加的黑边）
    return geometry().position(point.x() * ratio, point.y() * ratio, frameWidth_, frameHeight_, event.x, event.y,
                               clamp)
           && rcsdk::displayPosition(event.x, event.y, session_->streamInfo(), clamp);
}

void RemoteView::send(const rcs_input_event &event)
{
    if (session_)
        session_->sendInput(event);
}

void RemoteView::releaseAll()
{
    rcs_input_event release = rcs_input_event_init();
    release.type = RCS_INPUT_RELEASE_ALL;
    send(release);
}

// 鼠标悬停移动（控制时才接收）。Qt Quick 在没有移动时也会每帧再报一次悬停
void RemoteView::hoverMoveEvent(QHoverEvent *hover)
{
    if (hover->position() == lastHover_)
        return;
    lastHover_ = hover->position();
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_MOVE;
    if (position(hover->position(), event, false))
        send(event);
}

// 按着键拖动：拖出画面时贴在边上
void RemoteView::mouseMoveEvent(QMouseEvent *mouse)
{
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_MOVE;
    if (inputEnabled_ && position(mouse->position(), event, true))
        send(event);
}

void RemoteView::button(QMouseEvent *mouse, bool down)
{
    forceActiveFocus(Qt::MouseFocusReason);
    // 接住这次按下，松开才会送到这里
    mouse->accept();
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_BUTTON;
    event.code = remote_input::button(mouse->button());
    event.down = down;
    if (inputEnabled_ && event.code && position(mouse->position(), event, !down))
        send(event);
}

void RemoteView::mousePressEvent(QMouseEvent *mouse)
{
    button(mouse, true);
}

void RemoteView::mouseReleaseEvent(QMouseEvent *mouse)
{
    button(mouse, false);
}

// 控制时滚动对方；不控制、原始大小时平移画面
void RemoteView::wheelEvent(QWheelEvent *wheel)
{
    if (!inputEnabled_) {
        if (!nativeSize_ || !frameWidth_)
            return;
        const qreal ratio = window()->effectiveDevicePixelRatio();
        panX_ = std::clamp(panX_ - wheel->angleDelta().x(), 0, std::max(0, int(frameWidth_) - int(width() * ratio)));
        panY_ = std::clamp(panY_ - wheel->angleDelta().y(), 0, std::max(0, int(frameHeight_) - int(height() * ratio)));
        updateCursor();
        update();
        return;
    }
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_WHEEL;
    const int lines = QGuiApplication::styleHints()->wheelScrollLines();
    event.wheel_x = wheelX_.take(wheel->angleDelta().x(), lines);
    event.wheel_y = wheelY_.take(wheel->angleDelta().y(), lines);
    if ((event.wheel_x || event.wheel_y) && position(wheel->position(), event, false))
        send(event);
}

void RemoteView::key(QKeyEvent *key, bool down)
{
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_KEY;
    event.code = remote_input::keyUsage(*key, remoteIsMac());
    event.down = down;
    event.repeat = key->isAutoRepeat();
    if (!inputEnabled_ || !event.code) {
        key->ignore();
        return;
    }
    send(event);
    key->accept();
}

void RemoteView::keyPressEvent(QKeyEvent *event)
{
    key(event, true);
}

void RemoteView::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat())
        key(event, false);
}

// 焦点离开（切到别的窗口、别的标签）：对方那里不能留着按住的键
void RemoteView::focusOutEvent(QFocusEvent *event)
{
    if (inputEnabled_)
        releaseAll();
    QQuickItem::focusOutEvent(event);
}

// 控制时本程序的快捷键也交给对方
bool RemoteView::event(QEvent *event)
{
    if (inputEnabled_ && event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QQuickItem::event(event);
}
