#include "remote_view.h"

#include "frame_mailbox.h"
#include "frame_node.h"
#ifdef Q_OS_IOS
#include "hardware_keys.h"
#endif
#include "viewer_session.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QGuiApplication>
#include <QLineF>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QStyleHints>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <span>

namespace {

// 本机指针换成了"对方的指针看不见"
constexpr uint32_t HiddenCursor = std::numeric_limits<uint32_t>::max();

// 最近拿到焦点的画面：打字框有焦点时，接着的键盘的按键给它（RemoteView::keyTarget）
QPointer<RemoteView> focusedView;

// 手机上的手指（逻辑像素、毫秒）
constexpr qreal TapSlop = 10;         // 动了不到这么多还算点
constexpr int LongPressMs = 450;      // 按住不放多久算长按
constexpr int DoubleTapMs = 350;      // 两下点得这么快、
constexpr qreal DoubleTapSlop = 28;   // 离得这么近，第二下落在第一下的地方（对方才认成双击）
constexpr qreal PinchSlop = 24;       // 两个手指的距离变了这么多算缩放
constexpr qreal ScrollSlop = 12;      // 两个手指一起动了这么多算滚动
constexpr qreal ScrollLinePx = 18;    // 两个手指滑多少滚一行
constexpr double TouchpadSpeed = 1.6; // 触摸板：手指动一点，光标在看到的画面上动这么多倍
constexpr QLatin1StringView Touchpad("touchpad");

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
    connect(this, &QQuickItem::widthChanged, this, &RemoteView::clampZoom);
    connect(this, &QQuickItem::heightChanged, this, &RemoteView::clampZoom);
    longPress_.setSingleShot(true);
    longPress_.setInterval(LongPressMs);
    connect(&longPress_, &QTimer::timeout, this, &RemoteView::longPressed);
#ifdef Q_OS_IOS
    hardware_keys::install();
#endif
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
    updateSystemKeys();
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
    const int viewWidth = int(width() * ratio);
    const int viewHeight = int(height() * ratio);
    rcsdk::ViewGeometry g = rcsdk::viewGeometry(viewWidth, viewHeight, frameWidth_, frameHeight_,
                                                nativeSize_ ? rcsdk::ViewScale::nativePixels : rcsdk::ViewScale::fit,
                                                panX_, panY_);
    if (zoom_ <= 1 || nativeSize_ || g.width <= 0)
        return g;
    // 手机上放大：放得下时的大小乘倍数，露出来的是条目那么大的一块（zoomX_、zoomY_ 是它在放大的画面里的左上角）；
    // 放大以后还比条目窄的那一边照旧居中
    const auto axis = [](double scaled, int view, double offset, uint32_t frame, int &x, int &size, int &source,
                         int &sourceSize) {
        if (scaled <= view) {
            size = int(scaled);
            x = (view - size) / 2;
            source = 0;
            sourceSize = int(frame);
            return;
        }
        x = 0;
        size = view;
        source = int(offset / scaled * frame);
        sourceSize = std::max(1, int(view / scaled * frame));
    };
    const double scaledWidth = g.width * zoom_;
    const double scaledHeight = g.height * zoom_;
    axis(scaledWidth, viewWidth, zoomX_, frameWidth_, g.x, g.width, g.sourceX, g.sourceWidth);
    axis(scaledHeight, viewHeight, zoomY_, frameHeight_, g.y, g.height, g.sourceY, g.sourceHeight);
    return g;
}

rcsdk::ViewGeometry RemoteView::fitGeometry() const
{
    const qreal ratio = window()->effectiveDevicePixelRatio();
    return rcsdk::viewGeometry(int(width() * ratio), int(height() * ratio), frameWidth_, frameHeight_);
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
        // 笔的事件 Qt Quick 不交给条目，只有窗口收得到（pen）
        value.window->installEventFilter(this);
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
    // 触摸板：没在用手指移的时候跟着对方报来的位置（对方那边的人也可能在动鼠标）
    if (touchpad() && !touching_ && session_ && session_->cursor().visible) {
        uint32_t x = session_->cursor().x;
        uint32_t y = session_->cursor().y;
        rcsdk::framePosition(x, y, session_->streamInfo());
        cursorX_ = x;
        cursorY_ = y;
    }
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

    // 控制时：本机的指针换成对方指针的样子（手机上没有本机的指针，和只看时一样画在画面上）
    if (inputEnabled_ && touchMode_.isEmpty()) {
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

    // 只看时、手机上：在画面上对方指针的位置画出来（触摸板：本机算出来的位置）
    restoreLocal();
    uint32_t x = cursor.x;
    uint32_t y = cursor.y;
    rcsdk::framePosition(x, y, info);
    if (touchpad()) {
        x = uint32_t(cursorX_);
        y = uint32_t(cursorY_);
    }
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

// 鼠标悬停移动（控制时才接收）。Qt Quick 在没有移动时也会每帧再报一次悬停（位置是最后一次指针在的地方）。
// 手机上 Qt Quick 把第一个手指当成指针，按着、抬起的地方都报悬停（两个手指先后抬起就是两处）：落在这次触摸的
// 手指最后在的地方的不算，不然触摸板的光标会跳过去；真的鼠标、触控板照常
void RemoteView::hoverMoveEvent(QHoverEvent *hover)
{
    if (hover->position() == lastHover_)
        return;
    lastHover_ = hover->position();
    for (const QPointF &point : std::as_const(touchPoints_)) {
        if (QLineF(point, hover->position()).length() < 1)
            return;
    }
    pointer_ = hover->position();
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
    // Qt 的横向正值是往左，SDK 的是往右
    event.wheel_x = wheelX_.take(-wheel->angleDelta().x(), lines);
    event.wheel_y = wheelY_.take(wheel->angleDelta().y(), lines);
    if ((event.wheel_x || event.wheel_y) && position(wheel->position(), event, false))
        send(event);
}

void RemoteView::key(QKeyEvent *key, bool down)
{
    const uint32_t usage = remote_input::keyUsage(*key, remoteIsMac());
    if (!inputEnabled_ || !usage) {
        key->ignore();
        return;
    }
    sendKey(usage, down, key->isAutoRepeat(), key->modifiers() == Qt::NoModifier);
    key->accept();
}

void RemoteView::sendKey(uint32_t usage, bool down, bool repeat, bool plain)
{
    if (!inputEnabled_)
        return;
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_KEY;
    event.code = usage;
    event.down = down;
    event.repeat = repeat;
#ifndef Q_OS_MACOS
    // 连 Mac：F 键当 MacBook 顶上那一排（Mac 自己的键盘上按 F 键本来就是这样，不用换）
    if (remoteIsMac() && plain) {
        if (const auto topRow = remote_input::macTopRow(event.code)) {
            if (topRow->consumer) {
                event.code = RCS_KEY_CONSUMER | topRow->consumer;
                send(event);
            } else if (down && !event.repeat) {
                rcs_input_event combo = rcs_input_event_init();
                combo.type = RCS_INPUT_KEY;
                for (const uint32_t usage : topRow->keys) {
                    combo.code = usage;
                    combo.down = 1;
                    send(combo);
                }
                for (auto usage = std::rbegin(topRow->keys); usage != std::rend(topRow->keys); ++usage) {
                    combo.code = *usage;
                    combo.down = 0;
                    send(combo);
                }
            }
            return;
        }
    }
#else
    Q_UNUSED(plain);
#endif
    send(event);
}

RemoteView *RemoteView::keyTarget()
{
    QObject *const focused = QGuiApplication::focusObject();
    RemoteView *view = qobject_cast<RemoteView *>(focused);
    if (!view && focused && focused->property("remoteKeys").toBool())
        view = focusedView;
    return view && view->inputEnabled_ && view->isVisible() ? view : nullptr;
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

void RemoteView::focusInEvent(QFocusEvent *event)
{
    focusedView = this;
    updateSystemKeys();
    QQuickItem::focusInEvent(event);
}

// 焦点离开（切到别的窗口、别的标签）：对方那里不能留着按住的键
void RemoteView::focusOutEvent(QFocusEvent *event)
{
    if (inputEnabled_)
        releaseAll();
    updateSystemKeys();
    QQuickItem::focusOutEvent(event);
}

void RemoteView::updateSystemKeys()
{
#ifdef Q_OS_WIN
    const bool wanted = inputEnabled_ && hasActiveFocus() && touchMode_.isEmpty();
    if (wanted == bool(systemKeys_))
        return;
    if (!wanted) {
        systemKeys_.reset();
        return;
    }
    systemKeys_ = std::make_unique<SystemKeys>([this](uint32_t usage, bool down) {
        rcs_input_event event = rcs_input_event_init();
        event.type = RCS_INPUT_KEY;
        event.code = usage;
        event.down = down;
        send(event);
    });
#endif
}

bool RemoteView::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
        return pen(static_cast<QTabletEvent *>(event));
    default:
        return QQuickItem::eventFilter(watched, event);
    }
}

// 笔在画面上：对方收得了笔时（rcs_peer_info.input_types）按笔发，画图的程序收得到压力、倾斜；
// 不然不管，Qt 把它当鼠标
bool RemoteView::pen(QTabletEvent *tablet)
{
    const QPointF point = mapFromScene(tablet->scenePosition());
    if (!inputEnabled_ || !isVisible() || !contains(point) || !session_
        || !(session_->peerInputTypes() & (1u << RCS_INPUT_PEN)))
        return false;
    tablet->accept(); // 不再变成鼠标
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_PEN;
    if (!position(point, event, true))
        return true;
    const bool touching = tablet->type() != QEvent::TabletRelease && (tablet->buttons() & Qt::LeftButton);
    event.down = touching;
    event.pressure = touching ? uint32_t(std::clamp(tablet->pressure() * 1024, 1.0, 1024.0)) : 0;
    event.tilt_x = int32_t(std::clamp(tablet->xTilt(), -90.0, 90.0));
    event.tilt_y = int32_t(std::clamp(tablet->yTilt(), -90.0, 90.0));
    event.rotation = uint32_t((int(tablet->rotation()) % 360 + 360) % 360);
    if (tablet->pointerType() == QPointingDevice::PointerType::Eraser)
        event.pen |= RCS_PEN_ERASER;
    send(event);
    // 抬起来了：对方那里也离开感应范围
    if (tablet->type() == QEvent::TabletRelease) {
        event.pen |= RCS_PEN_LEAVE;
        send(event);
    }
    return true;
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

// ---- 手机 ----

bool RemoteView::touchpad() const
{
    return touchMode_ == Touchpad;
}

void RemoteView::setTouchMode(const QString &mode)
{
    if (mode == touchMode_)
        return;
    touchMode_ = mode;
    // 收手指（不收的话 Qt 把手指当成鼠标）
    setAcceptTouchEvents(!mode.isEmpty());
    emit touchModeChanged();
    updateCursor();
}

void RemoteView::sendText(const QString &text)
{
    if (!inputEnabled_)
        return;
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_TEXT;
    for (const char32_t c : text.toUcs4()) {
        event.code = c;
        send(event);
    }
}

void RemoteView::pressKey(const QString &key, const QStringList &modifiers)
{
    const uint32_t usage = remote_input::namedKeyUsage(key);
    if (!inputEnabled_ || !usage)
        return;
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_KEY;
    event.down = 1;
    for (const QString &modifier : modifiers) {
        event.code = remote_input::namedKeyUsage(modifier);
        send(event);
    }
    event.code = usage;
    send(event);
    event.down = 0;
    send(event);
    for (auto modifier = modifiers.rbegin(); modifier != modifiers.rend(); ++modifier) {
        event.code = remote_input::namedKeyUsage(*modifier);
        send(event);
    }
}

void RemoteView::resetZoom()
{
    zoomAt(QPointF(width() / 2, height() / 2), 1);
}

// 最多放大到画面的一个像素占屏幕上两个像素
qreal RemoteView::maxZoom() const
{
    const rcsdk::ViewGeometry fit = fitGeometry();
    return fit.width > 0 ? std::max(2.0, 2.0 * frameWidth_ / fit.width) : 1.0;
}

// 放大到 zoom 倍，focus（条目坐标）下面的画面不动
void RemoteView::zoomAt(QPointF focus, qreal zoom)
{
    const rcsdk::ViewGeometry before = geometry();
    const rcsdk::ViewGeometry fit = fitGeometry();
    if (before.width <= 0 || fit.width <= 0)
        return;
    const qreal ratio = window()->effectiveDevicePixelRatio();
    const QPointF at = focus * ratio;
    // 手指下面是帧上的哪一点
    const double frameX = before.sourceX + (at.x() - before.x) * before.sourceWidth / before.width;
    const double frameY = before.sourceY + (at.y() - before.y) * before.sourceHeight / before.height;
    const qreal zoomed = std::clamp(zoom, 1.0, maxZoom());
    zoomX_ = frameX / frameWidth_ * fit.width * zoomed - at.x();
    zoomY_ = frameY / frameHeight_ * fit.height * zoomed - at.y();
    if (!qFuzzyCompare(zoomed, zoom_)) {
        zoom_ = zoomed;
        emit zoomChanged();
    }
    clampZoom();
}

void RemoteView::panBy(QPointF delta)
{
    const qreal ratio = window()->effectiveDevicePixelRatio();
    zoomX_ -= delta.x() * ratio;
    zoomY_ -= delta.y() * ratio;
    clampZoom();
}

// 露出来的那一块不出放大的画面。还没放进窗口时（QML 先定大小）不用管
void RemoteView::clampZoom()
{
    if (!window())
        return;
    const rcsdk::ViewGeometry fit = fitGeometry();
    const qreal ratio = window()->effectiveDevicePixelRatio();
    zoomX_ = std::clamp(zoomX_, 0.0, std::max(0.0, fit.width * zoom_ - width() * ratio));
    zoomY_ = std::clamp(zoomY_, 0.0, std::max(0.0, fit.height * zoom_ - height() * ratio));
    updateCursor();
    update();
}

void RemoteView::touchEvent(QTouchEvent *event)
{
    if (touchMode_.isEmpty()) {
        QQuickItem::touchEvent(event);
        return;
    }
    event->accept();
    if (event->type() == QEvent::TouchBegin)
        touchPoints_.clear();
    for (const QEventPoint &point : event->points())
        touchPoints_.insert(point.id(), point.position());
    if (event->type() == QEvent::TouchCancel) {
        if (gesture_.kind == Gesture::Kind::Drag)
            touchButton(RCS_BUTTON_LEFT, false, gesture_.last);
        else if (gesture_.kind == Gesture::Kind::Pointer)
            pointerButton(false, gesture_.last);
        longPress_.stop();
        touching_ = false;
        return;
    }
    QList<QPointF> pressed;
    for (const QEventPoint &point : event->points()) {
        if (point.state() != QEventPoint::Released)
            pressed.append(point.position());
    }
    if (event->type() == QEvent::TouchBegin && !pressed.isEmpty())
        beginTouch(pressed.front());
    if (pressed.size() >= 2)
        twoFingers(pressed.at(0), pressed.at(1));
    else if (pressed.size() == 1)
        oneFinger(pressed.front());
    else
        endTouch();
}

void RemoteView::beginTouch(QPointF point)
{
    // 接着的键盘按键给对方；输入框有焦点（打字时的系统键盘、侧边面板的对话）时不抢
    const QQuickItem *focused = window()->activeFocusItem();
    if (!focused || !(focused->flags() & ItemAcceptsInputMethod))
        forceActiveFocus(Qt::MouseFocusReason);
    touching_ = true;
    gesture_ = {};
    gesture_.start = point;
    gesture_.last = point;
    // iPad 上鼠标按下：落在指针那里，直接按下左键
    if (inputEnabled_ && pointer_ && QLineF(point, *pointer_).length() < 2) {
        gesture_.kind = Gesture::Kind::Pointer;
        pointerButton(true, point);
        return;
    }
    longPress_.start();
}

void RemoteView::oneFinger(QPointF point)
{
    using Kind = Gesture::Kind;
    if (gesture_.kind == Kind::Pointer) {
        gesture_.last = point;
        rcs_input_event event = rcs_input_event_init();
        event.type = RCS_INPUT_MOVE;
        if (position(point, event, true))
            send(event);
        return;
    }
    if (gesture_.kind == Kind::Pending) {
        if (QLineF(point, gesture_.start).length() < TapSlop)
            return;
        longPress_.stop();
        if (!inputEnabled_) {
            gesture_.kind = Kind::Pan;
        } else if (touchpad()) {
            gesture_.kind = Kind::Move;
        } else {
            // 直接点：从按下的地方按着左键拖
            gesture_.kind = Kind::Drag;
            touchButton(RCS_BUTTON_LEFT, true, gesture_.start);
        }
    }
    const QPointF delta = point - gesture_.last;
    gesture_.last = point;
    switch (gesture_.kind) {
    case Kind::Move:
        moveCursor(delta);
        break;
    case Kind::Drag:
        if (touchpad()) {
            moveCursor(delta);
        } else {
            rcs_input_event event = rcs_input_event_init();
            event.type = RCS_INPUT_MOVE;
            if (position(point, event, true))
                send(event);
        }
        break;
    case Kind::Pan:
        panBy(delta);
        break;
    default:
        // 两个手指的操作里抬起了一个：等另一个也抬起
        break;
    }
}

void RemoteView::twoFingers(QPointF a, QPointF b)
{
    using Kind = Gesture::Kind;
    const QPointF center = (a + b) / 2;
    const qreal distance = QLineF(a, b).length();
    switch (gesture_.kind) {
    case Kind::Pending:
    case Kind::Move:
    case Kind::Pan:
        // 第二个手指按下
        longPress_.stop();
        gesture_.kind = Kind::TwoPending;
        gesture_.twoStart = center;
        gesture_.last = center;
        gesture_.startDistance = std::max<qreal>(1, distance);
        gesture_.startZoom = zoom_;
        return;
    case Kind::TwoPending:
        if (std::abs(distance - gesture_.startDistance) > PinchSlop)
            gesture_.kind = Kind::Pinch;
        else if (QLineF(center, gesture_.twoStart).length() > ScrollSlop)
            gesture_.kind = inputEnabled_ ? Kind::Scroll : Kind::Pinch;
        else
            return;
        break;
    case Kind::Scroll:
    case Kind::Pinch:
        break;
    default:
        // 拖着的时候按下的第二个手指不算
        return;
    }
    const QPointF delta = center - gesture_.last;
    gesture_.last = center;
    if (gesture_.kind == Kind::Pinch) {
        zoomAt(center, gesture_.startZoom * distance / gesture_.startDistance);
        panBy(delta);
    } else {
        scrollBy(delta, center);
    }
}

void RemoteView::endTouch()
{
    using Kind = Gesture::Kind;
    longPress_.stop();
    if (inputEnabled_) {
        switch (gesture_.kind) {
        case Kind::Pending:
            tap(gesture_.start);
            break;
        case Kind::TwoPending:
            // 两个手指点一下：右键
            touchButton(RCS_BUTTON_RIGHT, true, gesture_.twoStart);
            touchButton(RCS_BUTTON_RIGHT, false, gesture_.twoStart);
            break;
        case Kind::Drag:
            touchButton(RCS_BUTTON_LEFT, false, gesture_.last);
            break;
        case Kind::Pointer:
            pointerButton(false, gesture_.last);
            break;
        default:
            break;
        }
    }
    touching_ = false;
    gesture_ = {};
    updateCursor();
}

// 按住不放：触摸板上按下左键（接着拖），直接点是右键
void RemoteView::longPressed()
{
    if (!inputEnabled_ || gesture_.kind != Gesture::Kind::Pending)
        return;
    emit pressedAndHeld();
    if (touchpad()) {
        gesture_.kind = Gesture::Kind::Drag;
        touchButton(RCS_BUTTON_LEFT, true, gesture_.start);
        return;
    }
    gesture_.kind = Gesture::Kind::Done;
    touchButton(RCS_BUTTON_RIGHT, true, gesture_.start);
    touchButton(RCS_BUTTON_RIGHT, false, gesture_.start);
}

void RemoteView::tap(QPointF point)
{
    // 点两下：第二下落在第一下的地方
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastTapTime_ < DoubleTapMs && QLineF(point, lastTapPoint_).length() < DoubleTapSlop)
        point = lastTapPoint_;
    lastTapPoint_ = point;
    lastTapTime_ = now;
    touchButton(RCS_BUTTON_LEFT, true, point);
    touchButton(RCS_BUTTON_LEFT, false, point);
}

// 两个手指滑：内容跟着手指走（手指往上，内容往上，是往下滚）
void RemoteView::scrollBy(QPointF delta, QPointF at)
{
    gesture_.scrollRest += delta;
    const int linesX = int(gesture_.scrollRest.x() / ScrollLinePx);
    const int linesY = int(gesture_.scrollRest.y() / ScrollLinePx);
    if (!linesX && !linesY)
        return;
    gesture_.scrollRest -= QPointF(linesX * ScrollLinePx, linesY * ScrollLinePx);
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_WHEEL;
    event.wheel_x = -linesX;
    event.wheel_y = linesY;
    if (touchTarget(at, event))
        send(event);
}

bool RemoteView::touchTarget(QPointF point, rcs_input_event &event) const
{
    if (!touchpad())
        return position(point, event, true);
    if (!frameWidth_ || !session_)
        return false;
    event.x = uint32_t(cursorX_);
    event.y = uint32_t(cursorY_);
    return rcsdk::displayPosition(event.x, event.y, session_->streamInfo(), true);
}

void RemoteView::touchButton(uint32_t button, bool down, QPointF point)
{
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_BUTTON;
    event.code = button;
    event.down = down;
    if (touchTarget(point, event))
        send(event);
}

void RemoteView::pointerButton(bool down, QPointF point)
{
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_BUTTON;
    event.code = RCS_BUTTON_LEFT;
    event.down = down;
    if (position(point, event, !down))
        send(event);
}

// 触摸板：手指动了 delta（条目坐标），光标在看到的画面上动 TouchpadSpeed 倍
void RemoteView::moveCursor(QPointF delta)
{
    const rcsdk::ViewGeometry g = geometry();
    if (g.width <= 0 || g.height <= 0 || !frameWidth_ || !frameHeight_)
        return;
    const qreal ratio = window()->effectiveDevicePixelRatio();
    // 条目上的一个物理像素是帧上的几个像素，再换成 0..65535
    const double dx = delta.x() * ratio * g.sourceWidth / g.width * TouchpadSpeed;
    const double dy = delta.y() * ratio * g.sourceHeight / g.height * TouchpadSpeed;
    cursorX_ = std::clamp(cursorX_ + dx / std::max(1u, frameWidth_ - 1) * 65535, 0.0, 65535.0);
    cursorY_ = std::clamp(cursorY_ + dy / std::max(1u, frameHeight_ - 1) * 65535, 0.0, 65535.0);
    rcs_input_event event = rcs_input_event_init();
    event.type = RCS_INPUT_MOVE;
    if (touchTarget({}, event))
        send(event);
    keepCursorInView();
    updateCursor();
}

// 放大了的时候光标走到边上：画面跟着移，光标留在条目里（离边至少六分之一）
void RemoteView::keepCursorInView()
{
    if (zoom_ <= 1)
        return;
    const rcsdk::ViewGeometry g = geometry();
    double px = 0;
    double py = 0;
    const auto inside = [&] {
        return g.point(uint32_t(cursorX_), uint32_t(cursorY_), frameWidth_, frameHeight_, px, py);
    };
    const qreal ratio = window()->effectiveDevicePixelRatio();
    const double viewWidth = width() * ratio;
    const double viewHeight = height() * ratio;
    if (!inside()) {
        // 光标在露出来的那一块外面：把它放到正中
        const rcsdk::ViewGeometry fit = fitGeometry();
        zoomX_ = cursorX_ / 65535 * fit.width * zoom_ - viewWidth / 2;
        zoomY_ = cursorY_ / 65535 * fit.height * zoom_ - viewHeight / 2;
        clampZoom();
        return;
    }
    const double marginX = viewWidth / 6;
    const double marginY = viewHeight / 6;
    if (px < marginX)
        zoomX_ -= marginX - px;
    else if (px > viewWidth - marginX)
        zoomX_ += px - (viewWidth - marginX);
    if (py < marginY)
        zoomY_ -= marginY - py;
    else if (py > viewHeight - marginY)
        zoomY_ += py - (viewHeight - marginY);
    clampZoom();
}
