#pragma once

#include "remote_input.h"
#include "viewer_session.h"
#ifdef Q_OS_WIN
#include "system_keys_win.h"
#endif

#include <rcsdk/view_geometry.hpp>

#include <QHash>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

class FrameMailbox;
class QQuickPaintedItem;
class QTabletEvent;

// 会话窗口里的远程画面（QML 元素）：画观看会话解出的帧，控制时把本机的鼠标、键盘发给对方。
// GPU 上的帧由 FrameNode 直接画，CPU 上的图像（显卡建不起解码设备时）上传成纹理画。
// 对方的指针单独传来：控制时本机的指针换成它的样子（跟手，没有延迟），只看时在画面上画出来；
// 关掉"显示远程光标"时都不要，只有本机自己的指针。
// 手机（touchMode）：没有本机的指针，对方的指针一直画在画面上。手指的操作：
// - 触摸板：一个手指滑动移光标，点一下是点击（点两下双击），按住不放再拖是按着左键拖，两个手指点一下是右键
// - 直接点：点在哪就点哪，拖就是按着左键拖，按住不放是右键
// - 都是：两个手指上下左右滑是滚轮，两个手指张开、捏拢放大缩小画面（放大以后两个手指拖着移动）
// 接了鼠标、触控板也照常：Android 上就是鼠标；iPad 上移动报成悬停、按下报成触摸，落在指针那里的触摸当鼠标按键。
// 接了键盘：点一下画面键盘就给对方（正在打字时不抢系统键盘的焦点）
class RemoteView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(ViewerSession *session READ session WRITE setSession NOTIFY sessionChanged)
    // 原始大小（一个画面像素对一个屏幕像素，不控制时滚轮平移）；否则适应窗口、留黑边
    Q_PROPERTY(bool nativeSize READ nativeSize WRITE setNativeSize NOTIFY nativeSizeChanged)
    // 把本机的输入发给对方；关掉时松开对方那里按着的键
    Q_PROPERTY(bool inputEnabled READ inputEnabled WRITE setInputEnabled NOTIFY inputEnabledChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(bool remoteCursor READ remoteCursor WRITE setRemoteCursor NOTIFY remoteCursorChanged)
    // 会话窗口"发送快捷键"菜单：对方系统上的组合键 [{id, label, keys}]，连上以后按对方的系统
    Q_PROPERTY(QVariantList shortcuts READ shortcuts NOTIFY shortcutsChanged)
    // 对方是 Mac（手机的按键栏上写 ⌘ 还是 Win）
    Q_PROPERTY(bool remoteMac READ remoteIsMac NOTIFY shortcutsChanged)
    // 手机：手指怎么操作对方，touchpad | direct；空：电脑上（没有手指）
    Q_PROPERTY(QString touchMode READ touchMode WRITE setTouchMode NOTIFY touchModeChanged)
    // 手机上放大的倍数：1 是整个画面放得下
    Q_PROPERTY(qreal zoom READ zoom NOTIFY zoomChanged)
    // 全屏（只有画面，别的都藏起来）：Ctrl+Alt+Shift+F（Mac 上 Control、Command 都算）、四个以上手指按住 3 秒
    // 发 exitFullScreenRequested，这些不发给对方
    Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY fullScreenChanged)

public:
    explicit RemoteView(QQuickItem *parent = nullptr);
    ~RemoteView() override;

    ViewerSession *session() const { return session_; }
    void setSession(ViewerSession *session);
    bool nativeSize() const { return nativeSize_; }
    void setNativeSize(bool nativeSize);
    bool inputEnabled() const { return inputEnabled_; }
    void setInputEnabled(bool enabled);
    bool hasFrame() const { return hasFrame_; }
    bool remoteCursor() const { return remoteCursor_; }
    void setRemoteCursor(bool remoteCursor);
    QVariantList shortcuts() const;

    // 按下又松开 shortcuts 里 id 那一项
    Q_INVOKABLE void sendShortcut(const QString &id);

    QString touchMode() const { return touchMode_; }
    void setTouchMode(const QString &mode);
    qreal zoom() const { return zoom_; }
    // 手机上打的字（输入法定下来的），一个字一个字发给对方
    Q_INVOKABLE void sendText(const QString &text);
    // 手机的按键栏：按一下 key，同时按着 modifiers（名字见 remote_input::namedKeyUsage）
    Q_INVOKABLE void pressKey(const QString &key, const QStringList &modifiers = {});
    // 回到整个画面放得下
    Q_INVOKABLE void resetZoom();

    // 键盘上的一个键（USB HID 键码）发给对方。plain：没按着修饰键（连 Mac 时 F 键当顶上那一排）
    void sendKey(uint32_t usage, bool down, bool repeat, bool plain);
    // 接着的键盘的按键现在给哪个画面（iPad 上不经过 Qt，hardware_keys_ios.mm）：有焦点、在控制的那个；
    // 它的打字框（TouchKeyboard 里 remoteKeys 为真的那个）有焦点时也给它
    static RemoteView *keyTarget();
    // 对方是 Mac（连上以后才知道）
    bool remoteIsMac() const;
    bool fullScreen() const { return fullScreen_; }
    void setFullScreen(bool fullScreen);
    // 有焦点的画面（或者它的打字框有焦点）：在不在控制都算
    static RemoteView *focused();
    // 退出全屏的组合键：F，同时按着 Shift、Alt（Option）和 Control 或 Command
    static bool isExitFullScreenChord(int key, Qt::KeyboardModifiers modifiers);

signals:
    void sessionChanged();
    void nativeSizeChanged();
    void inputEnabledChanged();
    void hasFrameChanged();
    void remoteCursorChanged();
    void shortcutsChanged();
    void touchModeChanged();
    void zoomChanged();
    // 手机：按住不放起了作用（触摸板开始拖、直接点出了右键），手机震一下
    void pressedAndHeld();
    void fullScreenChanged();
    // 全屏时按了退出的组合键、四个以上手指按住了 3 秒
    void exitFullScreenRequested();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

private:
    enum class NodeKind { None, Frame, Image };

    // 手机：手指的一次操作（第一个手指按下到全部抬起）。pending：刚按下，还不知道是点还是拖 | move：触摸板上移光标 |
    // drag：按着左键拖 | twoPending：两个手指，还不知道是缩放还是滚动 | scroll | pinch：缩放、移动画面 |
    // pan：只看时一个手指移动画面 | done：这一次处理完了（按住不放出了右键） | pointer：iPad 上鼠标按着左键
    struct Gesture
    {
        enum class Kind { Pending, Move, Drag, TwoPending, Scroll, Pinch, Pan, Done, Pointer };
        Kind kind = Kind::Pending;
        QPointF start; // 第一个手指按下的地方（条目坐标）
        QPointF last;  // 上一次一个手指在哪、两个手指的中点
        QPointF twoStart;
        qreal startDistance = 0;
        qreal startZoom = 1;
        QPointF scrollRest; // 滚动还没攒够一行的
    };

    // 画面在条目里的位置（物理像素）；画和输入用同一个
    rcsdk::ViewGeometry geometry() const;
    // 条目里的一点 → 对方显示器上的位置；在画面外或黑边上时 false（clamp：贴到边上）
    bool position(QPointF point, rcs_input_event &event, bool clamp) const;
    // 笔（Apple Pencil、数位板）：对方收得了笔时按笔发，返回 true；不然 false，Qt 把它当鼠标
    bool pen(QTabletEvent *tablet);
    void send(const rcs_input_event &event);
    // 控制对方、有焦点时，系统自己先拿走的键也给对方：Windows 截下 Win 键（SystemKeys），macOS 暂停系统的快捷键
    // （system_keys_mac）
    void updateSystemKeys();
    void releaseAll();
    void button(QMouseEvent *mouse, bool down);
    void key(QKeyEvent *key, bool down);
    void updateCursor();
    bool touchpad() const;
    // 整个画面放得下时的位置（放大以前）
    rcsdk::ViewGeometry fitGeometry() const;
    qreal maxZoom() const;
    void zoomAt(QPointF focus, qreal zoom);
    void panBy(QPointF delta);
    void clampZoom();
    void beginTouch(QPointF point);
    void oneFinger(QPointF point);
    void twoFingers(QPointF a, QPointF b);
    void endTouch();
    void longPressed();
    // 三个以上手指：不是给对方的操作（按着的键松开），四个以上按住 3 秒退出全屏
    void manyFingers(qsizetype count);
    void tap(QPointF point);
    void scrollBy(QPointF delta, QPointF at);
    // 手机上的输入落在哪：触摸板是光标，直接点是手指（point）
    bool touchTarget(QPointF point, rcs_input_event &event) const;
    void touchButton(uint32_t button, bool down, QPointF point);
    // iPad 上的鼠标：左键按在 point
    void pointerButton(bool down, QPointF point);
    void moveCursor(QPointF delta);
    void keepCursorInView();
    // 同步时：画面有没有，界面线程上再通知
    void publishHasFrame(bool hasFrame);

    QPointer<ViewerSession> session_;
    std::shared_ptr<FrameMailbox> frames_;
    bool nativeSize_ = false;
    bool inputEnabled_ = false;
#ifdef Q_OS_WIN
    std::unique_ptr<SystemKeys> systemKeys_;
#endif
    bool remoteCursor_ = true;
    int panX_ = 0;
    int panY_ = 0;
    QPointF lastHover_;
    remote_input::WheelLines wheelX_;
    remote_input::WheelLines wheelY_;

    // 同步时写（界面线程停着），界面线程读
    NodeKind nodeKind_ = NodeKind::None;
    uint32_t frameWidth_ = 0;
    uint32_t frameHeight_ = 0;
    qint64 imageKey_ = 0; // 已经上传的那张图像
    bool hasFrame_ = false;

    QQuickPaintedItem *cursorOverlay_; // 只看时画对方的指针
    // 控制时本机指针换成了对方的哪个形状（或藏起来）；空：本机自己的指针
    std::optional<uint32_t> localCursorShape_;
    double localCursorScale_ = 0;

    QString touchMode_;
    Gesture gesture_;
    bool touching_ = false;
    QHash<int, QPointF> touchPoints_; // 这一次触摸里每个手指最后在哪（hoverMoveEvent）
    std::optional<QPointF> pointer_;  // 真的鼠标、触控板的指针最后在哪（不是手指带出来的悬停）
    QTimer longPress_;
    QTimer manyFingersHeld_; // 四个以上手指按住多久了（全屏时退出）
    bool fullScreen_ = false;
    bool exitChordDown_ = false; // 退出全屏的组合键的 F 按着：松开也不发给对方
    // 触摸板：光标在编码帧上的位置（0..65535），手指移动时本机先算，不等对方报回来
    double cursorX_ = 32767;
    double cursorY_ = 32767;
    // 上一次点在哪、什么时候（点两下要落在同一个地方，对方才认成双击）
    QPointF lastTapPoint_;
    qint64 lastTapTime_ = 0;
    // 放大：倍数，放大以后的画面里条目的左上角在哪（物理像素）
    qreal zoom_ = 1;
    double zoomX_ = 0;
    double zoomY_ = 0;
};
