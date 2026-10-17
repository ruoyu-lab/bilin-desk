#include "viewer_session.h"

#include "devices/device_wake.h"
#include "service_client.h"
#include "view_quality.h"

#include <rc/device_id.h>
#include <rc/network_path.h>
#include <rc/control_protocol.h>
#include <rc/peer_protocol.h>
#include <rc/power_actions.h>
#include <rc/protocol.h>

#include <rcsdk/gpu.hpp>
#include <rcsdk/rcsdk.hpp>
#include <rcsdk/tuning.hpp>

#include <QHostAddress>
#include <QUuid>

#include <algorithm>
#include <optional>

namespace protocol = rc::protocol;

namespace {

// 对方不接受连接时会回绝；一直不回答多半是路上丢了
constexpr int AnswerMs = 15000;
// 有中继可以改走时，直连的连通检查等多久：通的路一两秒就找到，不用等 SDK 默认的 15 秒才知道不通
constexpr uint32_t DirectCheckMs = 5000;
// 对方重启、注销以后等它回来多久；它在线却连不上时隔多久再连（服务器限着连不上的次数，不在线时不试）
constexpr int RestartWaitMs = 5 * 60 * 1000;
constexpr int SignOutWaitMs = 2 * 60 * 1000;
// 发了唤醒包以后等它上线：从睡眠里醒来很快，从关机开机要一两分钟
constexpr int WakeWaitMs = 3 * 60 * 1000;
// 等对方回来时 powerWait_ 的 action：电源模块的那几样以外，还有本机叫醒它
constexpr QLatin1StringView Waking{"wake"};
constexpr int PowerRetryMs = 5 * 1000;

QString codecName(rcs_codec codec)
{
    switch (codec) {
    case RCS_CODEC_H264: return QStringLiteral("H.264");
    case RCS_CODEC_HEVC: return QStringLiteral("HEVC");
    case RCS_CODEC_AV1: return QStringLiteral("AV1");
    case RCS_CODEC_VP9: return QStringLiteral("VP9");
    default: return {};
    }
}

// SDK 的定长密钥（rcsdk::DeviceKey、rcsdk::PublicKey），bytes 的长度和它一样
template<typename Key>
Key keyFrom(const QByteArray &bytes)
{
    Key key;
    std::copy(bytes.cbegin(), bytes.cend(), key.begin());
    return key;
}

QString connectionTypeName(rc::NetworkPath path)
{
    switch (path) {
    case rc::NetworkPath::Lan: return ViewerSession::tr("局域网直连");
    case rc::NetworkPath::Direct: return ViewerSession::tr("直连");
    case rc::NetworkPath::Relay: return ViewerSession::tr("中继");
    default: return {};
    }
}

} // namespace

ViewerSession::ViewerSession(const QString &target, ServiceClient &service, void *decodeDevice,
                             const LocalCodecs &codecs, Conversation &conversation, rc::InterruptedTransfers &transfers,
                             std::function<QByteArray(const QString &)> mineKey, QObject *parent)
    : QObject(parent)
    , service_(service)
    , decodeDevice_(decodeDevice)
    , deviceId_(rc::DeviceId::parse(target).digits())
    , address_(deviceId_.isEmpty() && !QHostAddress(target.trimmed()).isNull() ? target.trimmed() : QString())
    , target_(deviceId_.isEmpty() ? address_ : deviceId_)
    , mineKey_(std::move(mineKey))
    , chat_(*this, conversation)
    , displays_(new RemoteDisplays([this](const QByteArray &message) { sendControl(message); }, this))
    , files_(new ViewerFiles(transfers, target_, this))
    , power_(new ViewerPower(this))
    , codecs_(codecs)
{
    channels_.add(rc::peer::ChatChannel, [this](int, const QByteArray &data) { chat_.receive(data); });
    channels_.add(rc::peer::ControlChannel, [this](int, const QByteArray &data) {
        if (const std::optional<quint32> hardware = rc::control::parseEncoders(data)) {
            peerHardware_ = *hardware;
            emit qualityChanged();
            requestQuality();
        } else if (const std::optional<bool> main = rc::control::parseMainViewer(data)) {
            setMainViewer(*main);
        } else if (const std::optional<QString> sound = rc::control::parseSound(data)) {
            if (audio_.setPeerProblem(*sound))
                emit optionsChanged();
        } else if (const std::optional<QByteArray> wallpaper = rc::control::parseWallpaper(data)) {
            emit wallpaperReceived(*wallpaper);
        } else {
            displays_->received(data);
        }
    });
    connect(power_, &ViewerPower::changed, this, &ViewerSession::powerChanged);
    // 我的设备告诉了它的网卡：记着，它睡眠、关机以后好叫醒（按 IP 连的不知道是哪台，不记）
    connect(power_, &ViewerPower::wakeInterfaces, this, [this](const QList<rcs_power_interface> &interfaces) {
        if (!deviceId_.isEmpty())
            DeviceWake::instance()->remember(deviceId_, interfaces);
    });
    connect(DeviceWake::instance(), &DeviceWake::changed, this, &ViewerSession::wakeChanged);
    // 本机的解码器列好了，等着的连接接着连
    connect(&codecs_, &LocalCodecs::readyChanged, this, [this] {
        if (waitingForCodecs_)
            open(relay_);
    });
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, [this] { open(false); });
    statsTimer_.setInterval(1000);
    connect(&statsTimer_, &QTimer::timeout, this, &ViewerSession::refreshStats);
    answerTimer_.setSingleShot(true);
    answerTimer_.setInterval(AnswerMs);
    connect(&answerTimer_, &QTimer::timeout, this, [this] {
        if (handledByPower())
            return;
        stop();
        setStatus(QStringLiteral("failed"), tr("对方没有回应，过一会儿再试"));
    });
    if (target_.isEmpty())
        setStatus(QStringLiteral("failed"), tr("要填 9 位设备 ID，或者同一网络里的 IP 地址"));
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic == protocol::IceTopic) {
            updateIceServers();
        } else if (topic == protocol::AccountTopic) {
            // 等着重启、注销的对方回来：账号里它刚上线就连
            const bool online = peerOnline();
            if (online && !peerWasOnline_ && !powerWait_.action.isEmpty() && !session_) {
                retryTimer_.stop();
                open(false);
            }
            peerWasOnline_ = online;
        }
    });
    // 我的设备不要密码，一打开就连：等打开它的界面先设好画质、声音这些（同一轮事件里）
    if (byDevice()) {
        status_ = QStringLiteral("connecting");
        QMetaObject::invokeMethod(this, [this] { open(false); }, Qt::QueuedConnection);
    }
}

ViewerSession::~ViewerSession()
{
    endSignal();
}

void ViewerSession::setResolutionChoice(int longSide)
{
    if (longSide < 0 || uint32_t(longSide) == resolutionChoice_)
        return;
    resolutionChoice_ = uint32_t(longSide);
    emit qualityChanged();
    if (session_)
        requestQuality();
}

void ViewerSession::setFrameRateChoice(int frameRate)
{
    if (frameRate < 0 || uint32_t(frameRate) == frameRateChoice_)
        return;
    frameRateChoice_ = uint32_t(frameRate);
    emit qualityChanged();
    if (session_)
        requestQuality();
}

QVariantList ViewerSession::resolutionOptions() const
{
    QVariantList options;
    for (const uint32_t side : view_quality::longSides(display_)) {
        rcs_view_request_config request = rcs_view_request_config_init();
        view_quality::apply(side, 0, 0, 0, 0, display_, request);
        const QString size = QStringLiteral("%1 × %2").arg(request.width).arg(request.height);
        options.append(QVariantMap{
            {QStringLiteral("label"), side == 0 ? tr("%1（原始）").arg(size) : size},
            {QStringLiteral("value"), int(side)},
        });
    }
    return options;
}

int ViewerSession::resolutionIndex() const
{
    return view_quality::longSideIndex(resolutionChoice_, display_);
}

QVariantList ViewerSession::frameRateOptions() const
{
    QVariantList options;
    for (const uint32_t rate : view_quality::frameRates(display_))
        options.append(QVariantMap{{QStringLiteral("label"), tr("%1 FPS").arg(rate)}, {QStringLiteral("value"), int(rate)}});
    return options;
}

int ViewerSession::frameRateIndex() const
{
    return view_quality::frameRateIndex(frameRateChoice_, display_);
}

void ViewerSession::setBitrateChoice(int mbps)
{
    if (mbps < 0 || uint32_t(mbps) == bitrateChoice_)
        return;
    bitrateChoice_ = uint32_t(mbps);
    emit qualityChanged();
    if (session_)
        requestQuality();
}

// 自动的那一档写上现在按分辨率、帧率算出来是多少
QVariantList ViewerSession::bitrateOptions() const
{
    QVariantList options;
    for (const uint32_t mbps : view_quality::bitrates()) {
        QString label = tr("%1 Mbps").arg(mbps);
        if (mbps == 0) {
            rcs_view_request_config request = rcs_view_request_config_init();
            view_quality::apply(resolutionChoice_, frameRateChoice_, 0, 0, 0, display_, request);
            label = request.bitrate_bps > 0 ? tr("自动（%1 Mbps）").arg(qRound(request.bitrate_bps / 1e6)) : tr("自动");
        }
        options.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), int(mbps)}});
    }
    return options;
}

int ViewerSession::bitrateIndex() const
{
    return view_quality::bitrateIndex(bitrateChoice_);
}

void ViewerSession::setCodecChoice(int codec)
{
    if (codec < 0 || rcs_codec(codec) == codecChoice_)
        return;
    codecChoice_ = rcs_codec(codec);
    emit qualityChanged();
    if (session_)
        requestQuality();
}

// 第一个是自动（写上自动挑的是哪一种）；两边都用显卡的有就只有它们，要用 CPU 的写明
QVariantList ViewerSession::codecOptions() const
{
    const QList<view_quality::Codec> list = codecChoices();
    if (list.isEmpty())
        return {};
    const auto label = [](const view_quality::Codec &codec) {
        return codec.hardware ? codecName(codec.codec) : tr("%1（CPU）").arg(codecName(codec.codec));
    };
    QVariantList options{QVariantMap{{QStringLiteral("label"), tr("自动（%1）").arg(label(list.first()))},
                                     {QStringLiteral("value"), 0}}};
    for (const view_quality::Codec &codec : list)
        options.append(QVariantMap{{QStringLiteral("label"), label(codec)}, {QStringLiteral("value"), int(codec.codec)}});
    return options;
}

int ViewerSession::codecIndex() const
{
    const QList<view_quality::Codec> list = codecChoices();
    for (int i = 0; i < list.size(); ++i) {
        if (codecChoice_ != 0 && list.at(i).codec == codecChoice_)
            return i + 1;
    }
    return list.isEmpty() ? -1 : 0;
}

void ViewerSession::setClearText(bool clearText)
{
    if (clearText == clearText_)
        return;
    clearText_ = clearText;
    emit qualityChanged();
    if (session_)
        requestQuality();
}

void ViewerSession::setLowLatency(bool lowLatency)
{
    if (lowLatency == lowLatency_)
        return;
    lowLatency_ = lowLatency;
    emit qualityChanged();
    if (!session_)
        return;
    if (session_->setPreset(lowLatency_ ? RCS_PRESET_LOW_LATENCY : RCS_PRESET_BALANCED) != RCS_OK)
        qWarning("换低延迟模式：%s", rcsdk::lastCallError().c_str());
    if (status_ == u"connected")
        tellLowLatency();
}

void ViewerSession::tellLowLatency()
{
    sendControl(rc::control::lowLatency(lowLatency_));
}

void ViewerSession::setAudio(bool audio)
{
    if (audio_.setEnabled(audio, session_.get()))
        emit optionsChanged();
}

void ViewerSession::setClipboard(bool clipboard)
{
    if (clipboard_.setEnabled(clipboard, session_.get()))
        emit optionsChanged();
}

void ViewerSession::connectWithPin(const QString &pin)
{
    pin_ = pin.trimmed();
    byPassword_ = true;
    open(false);
}

bool ViewerSession::byDevice() const
{
    return !byPassword_ && !mineKey_(deviceId_).isEmpty()
           && !protocol::DeviceState::fromJson(service_.topic(protocol::DeviceTopic)).deviceKey.isEmpty();
}

void ViewerSession::usePassword(const QString &reason)
{
    qInfo("连接 %s：对方不认这台设备，改用临时密码", qUtf8Printable(target_));
    byPassword_ = true;
    setStatus(QStringLiteral("needPin"), reason);
}

void ViewerSession::open(bool relay)
{
    stop();
    relay_ = relay;
    if (service_.state() != ServiceClient::State::Ready) {
        setStatus(QStringLiteral("failed"), tr("后台服务没有在运行"));
        return;
    }
    // 对方正连着本机：再反过来连它，两边的画面互相套着，越套越卡
    const auto share = protocol::ShareState::fromJson(service_.topic(protocol::ShareTopic));
    if (std::any_of(share.viewers.cbegin(), share.viewers.cend(),
                    [this](const protocol::SharePeer &viewer) { return viewer.deviceId == deviceId_; })) {
        setStatus(QStringLiteral("failed"), tr("对方正在连着本机，不能反过来连它"));
        return;
    }
    // 程序刚打开，本机的解码器还没列好：列好了再连（LocalCodecs），这时先显示正在连接
    waitingForCodecs_ = !codecs_.ready();
    if (waitingForCodecs_) {
        setStatus(connectingStatus());
        return;
    }
    rcsdk::SessionConfig config(RCS_ROLE_VIEW, lowLatency_ ? RCS_PRESET_LOW_LATENCY : RCS_PRESET_BALANCED);
    // 显卡解不了的格式在 CPU 上解（老显卡）；指针单独传来，控制时本机的指针就是它，跟手
    config.setAcceleration(RCS_ACCELERATION_HARDWARE_FIRST).setCursorMode(RCS_CURSOR_SEPARATE);
    // 编码格式四种都收，本机解得了的才提供（不带 SDK 的编码库：VP9 只在显卡、系统解得了时有），
    // 用哪一种按两边都用显卡的挑（codecs()）。
    // 画面格式 4:2:0 以外还收 4:4:4（文字更清楚时要）：Mac 上解出两个平面、Windows 上是 BGRA 纹理，FrameNode 都画得了
    config.setCodecs({RCS_CODEC_H264, RCS_CODEC_HEVC, RCS_CODEC_AV1, RCS_CODEC_VP9});
    config.setPictureFormats({RCS_PICTURE_420_8, RCS_PICTURE_444_8});
    const rc::IceServers ice = iceServers();
    useTransport(config, ice);
    if (!relay_ && ice.turn)
        config.extension<rcs_network_tuning_config>().ice_connect_ms = DirectCheckMs;
    audio_.configure(config);
    clipboard_.configure(config);
    power_->configure(config);
    // 还不知道对方的显示器多大：尺寸先由对方定，知道以后再按选的分辨率、帧率要（requestQuality）
    view_quality::apply(resolutionChoice_, frameRateChoice_, bitrateChoice_, 0, 0, {}, config.viewRequest());
    request_ = config.viewRequest();
    rc::FileTransfers::configure(config);
    // 解出的画面直接在显卡上画（FrameNode）；Windows 建不起解码用的 D3D11 设备时（ViewGraphics）用 CPU 上的图像
#ifdef Q_OS_WIN
    const bool gpuFrames = decodeDevice_ != nullptr;
    if (gpuFrames)
        config.use(rcsdk::gpuDevice(decodeDevice_));
#else
    const bool gpuFrames = true;
#endif
    // 我的设备：只认它的公钥，它也只认本机的；别人：临时密码
    if (byDevice()) {
        access_ = rc::DeviceAccess;
        const QByteArray ownKey = protocol::DeviceState::fromJson(service_.topic(protocol::DeviceTopic)).deviceKey;
        config.setSecret(RCS_SECRET_NONE)
            .setDeviceKey(keyFrom<rcsdk::DeviceKey>(ownKey))
            .setTrustedKeys({keyFrom<rcsdk::PublicKey>(mineKey_(deviceId_))});
    } else {
        access_ = rc::PasswordAccess;
        try {
            config.setPin(pin_.toStdString());
        } catch (const rcsdk::Error &) {
            setStatus(QStringLiteral("needPin"), tr("临时密码不对"));
            return;
        }
    }

    connection_ = QUuid::createUuid().toString(QUuid::Id128);
    rcsdk::Callbacks callbacks;
    // SDK 的线程上：搬到界面线程再处理。只处理这一次连接的：停掉、重连之后，上一次排着的不再算数
    const QString connection = connection_;
    const rc::ConnectionMode mode{relay_ ? rc::RelayTransport : rc::DirectTransport, access_};
    callbacks.onSignal = [this, connection, mode](const std::string &text, uint64_t) {
        QMetaObject::invokeMethod(this, [this, connection, mode, text = QString::fromStdString(text)] {
            if (connection != connection_)
                return;
            QJsonObject signal{{QStringLiteral("to"), deviceId_},
                               {QStringLiteral("address"), address_},
                               {QStringLiteral("session"), connection_},
                               {QStringLiteral("text"), text}};
            mode.write(signal);
            service_.send(protocol::Signal, signal);
        }, Qt::QueuedConnection);
    };
    callbacks.onEvent = [this, connection](const rcs_event &event) {
        if (auto file = rc::FileTransfers::read(event)) {
            QMetaObject::invokeMethod(this, [this, connection, file = std::move(*file)] {
                if (connection == connection_)
                    files_->handle(file);
            }, Qt::QueuedConnection);
            return;
        }
        if (event.type == RCS_EVENT_STATE) {
            const rcs_state state = event.state.value;
            QMetaObject::invokeMethod(this, [this, connection, state] {
                if (connection == connection_)
                    stateEvent(state);
            }, Qt::QueuedConnection);
        } else if (event.type == RCS_EVENT_CONTROL) {
            const bool granted = event.control.granted != 0;
            QMetaObject::invokeMethod(this, [this, connection, granted] {
                if (connection != connection_ || granted == controlGranted_)
                    return;
                controlGranted_ = granted;
                emit controlChanged();
            }, Qt::QueuedConnection);
        } else if (event.type == RCS_EVENT_ADMISSION) {
            const rcs_admission_state state = event.admission.state;
            const rcs_result result = event.admission.result;
            const QString message = QString::fromUtf8(event.admission.message);
            QMetaObject::invokeMethod(this, [this, connection, state, result, message] {
                if (connection == connection_)
                    admissionEvent(state, result, message);
            }, Qt::QueuedConnection);
        } else if (event.type == RCS_EVENT_STREAM) {
            const view_quality::Display display{event.stream.info.display_width, event.stream.info.display_height,
                                                event.stream.info.display_refresh_hz};
            QMetaObject::invokeMethod(this, [this, connection, display] {
                if (connection != connection_)
                    return;
                // 对方换了显示器：能选的分辨率、帧率跟着变
                if (display.width > 0
                    && (display.width != display_.width || display.height != display_.height
                        || display.refreshHz != display_.refreshHz)) {
                    display_ = display;
                    emit qualityChanged();
                    requestQuality();
                }
                emit streamChanged();
            }, Qt::QueuedConnection);
        } else if (event.type == RCS_EVENT_CURSOR) {
            const rcs_cursor_event &pointer = event.cursor;
            // 图像只在这次回调里有效：先复制
            std::optional<CursorShape> shape;
            if (const rcs_cursor_shape *s = pointer.shape) {
                const QImage pixels(s->pixels, int(s->width), int(s->height), int(s->stride_bytes), QImage::Format_ARGB32);
                shape = CursorShape{pixels.copy(), QPoint(int(s->hotspot_x), int(s->hotspot_y))};
            }
            const Cursor cursor{pointer.visible != 0, pointer.x, pointer.y, pointer.shape_id};
            QMetaObject::invokeMethod(this, [this, connection, cursor, shape = std::move(shape)] {
                if (connection != connection_)
                    return;
                if (shape)
                    cursorShapes_.insert(cursor.shapeId, *shape);
                cursor_ = cursor;
                emit cursorChanged();
            }, Qt::QueuedConnection);
        } else if (event.type == RCS_EVENT_AUDIO && event.audio.direction == RCS_AUDIO_RECEIVING) {
            const rcs_audio_event sound = event.audio;
            QMetaObject::invokeMethod(this, [this, connection, sound] {
                if (connection == connection_ && audio_.update(sound, *session_))
                    emit optionsChanged();
            }, Qt::QueuedConnection);
        } else if (const auto change = ViewerClipboard::read(event)) {
            QMetaObject::invokeMethod(this, [this, connection, change = *change] {
                if (connection == connection_ && clipboard_.update(change))
                    emit optionsChanged();
            }, Qt::QueuedConnection);
        } else if (const auto power = ViewerPower::read(event)) {
            QMetaObject::invokeMethod(this, [this, connection, power = *power] {
                if (connection == connection_)
                    power_->handle(power, *session_);
            }, Qt::QueuedConnection);
        }
    };
    // 读连接的线程上：复制下来就返回，到界面线程上再按通道号交出去
    callbacks.onData = [this, connection](const rcs_data_message &message) {
        const std::uint32_t channel = message.channel;
        QByteArray data(reinterpret_cast<const char *>(message.data), qsizetype(message.size));
        QMetaObject::invokeMethod(this, [this, connection, channel, data = std::move(data)] {
            if (connection == connection_)
                channels_.receive(channel, 0, data);
        }, Qt::QueuedConnection);
    };
    // 画面：GPU 上的帧不经界面线程直接画（形状没变时）；CPU 上的图像先复制（只在这次回调里有效）
    if (gpuFrames) {
        callbacks.onGpuFrame = [this](const rcsdk::GpuFrame &frame) {
            if (!frames_->putFrame(frame))
                notifyFrame();
        };
    } else {
        callbacks.onFrame = [this](const rcs_frame &frame) {
            const QImage image(frame.data, int(frame.width), int(frame.height), int(frame.stride_bytes),
                               QImage::Format_RGB32);
            frames_->putImage(image.copy());
            notifyFrame();
        };
    }

    try {
        session_ = std::make_unique<rcsdk::Session>(config, std::move(callbacks));
    } catch (const rcsdk::Error &error) {
        stop();
        setStatus(QStringLiteral("failed"), QString::fromUtf8(error.what()));
        return;
    }
    files_->attach(session_->handle());
    if (hidden_)
        session_->pause(RCS_PAUSE_VIDEO);
    setStatus(connectingStatus());
    if (session_->start() != RCS_OK) {
        const QString message = QString::fromStdString(session_->failure().message);
        stop();
        setStatus(QStringLiteral("failed"), message);
        return;
    }
    answerTimer_.start();
}

rc::IceServers ViewerSession::iceServers() const
{
    return rc::IceServers::fromJson(service_.topic(protocol::IceTopic));
}

void ViewerSession::useTransport(rcsdk::SessionConfig &config, const rc::IceServers &ice) const
{
    if (relay_) {
        const rc::TurnServer &turn = *ice.turn;
        config.setRelay(turn.server.toStdString(), turn.username.toStdString(), turn.password.toStdString(),
                        turn.tcp ? RCS_TURN_TCP : RCS_TURN_UDP);
    } else {
        config.setTransport(RCS_TRANSPORT_P2P).setStunServers(ice.stunServers());
    }
}

void ViewerSession::updateIceServers()
{
    const rc::IceServers ice = iceServers();
    if (!session_ || (relay_ && !ice.turn))
        return;
    rcsdk::SessionConfig config(RCS_ROLE_VIEW);
    useTransport(config, ice);
    if (session_->update(config.iceServers()) != RCS_OK)
        qWarning("连接 %s 换 P2P 和中继用的服务器：%s", qUtf8Printable(target_), rcsdk::lastCallError().c_str());
}

void ViewerSession::setHidden(bool hidden)
{
    if (hidden == hidden_)
        return;
    hidden_ = hidden;
    if (!session_)
        return;
    if (hidden)
        session_->pause(RCS_PAUSE_VIDEO);
    else
        session_->resume(RCS_PAUSE_VIDEO);
}

void ViewerSession::retry()
{
    if (target_.isEmpty())
        return;
    if (byDevice() || !pin_.isEmpty())
        open(false);
    else
        setStatus(QStringLiteral("needPin"));
}

void ViewerSession::stop()
{
    endSignal();
    statsTimer_.stop();
    answerTimer_.stop();
    files_->detach();
    session_.reset();
    frames_->clear();
    emit frameReady();
    cursor_ = {};
    cursorShapes_.clear();
    emit cursorChanged();
    display_ = {};
    peerHardware_.reset();
    emit qualityChanged();
    displays_->clear();
    setMainViewer(true);
    if (controlGranted_) {
        controlGranted_ = false;
        emit controlChanged();
    }
}

// 后台服务一直替这次连接转信令（连着时换了网络也要），结束了告诉它
void ViewerSession::endSignal()
{
    if (connection_.isEmpty())
        return;
    service_.send(protocol::EndSignal, {{QStringLiteral("session"), connection_}});
    connection_.clear();
}

void ViewerSession::setMainViewer(bool main)
{
    if (main == mainViewer_)
        return;
    mainViewer_ = main;
    emit optionsChanged();
}

void ViewerSession::receiveSignal(const QString &text)
{
    answerTimer_.stop();
    session_->signal(text.toStdString());
}

void ViewerSession::undeliverable(const QString &reason)
{
    if (handledByPower())
        return;
    stop();
    if (reason == protocol::NoServer)
        setStatus(QStringLiteral("failed"), tr("本机没连上服务器，过一会儿再试"));
    else if (reason == protocol::Unreachable)
        setStatus(QStringLiteral("failed"), tr("同一网络里连不上这台电脑：地址不对，或者对方没打开本程序"));
    else if (reason == protocol::TooMany)
        setStatus(QStringLiteral("failed"), tr("连得太频繁了，过几分钟再试"));
    else
        setStatus(QStringLiteral("failed"), tr("对方不在线，或者设备 ID 不对"));
}

void ViewerSession::declined(const QString &reason)
{
    qInfo("连接 %s 被回绝：%s", qUtf8Printable(target_), qUtf8Printable(reason));
    // 对方正经中继服务别人：本机也经中继
    if (reason == QLatin1StringView(rc::peer::TransportBusy) && !relay_ && iceServers().turn) {
        open(true);
        return;
    }
    if (handledByPower())
        return;
    stop();
    if (reason == QLatin1StringView(rc::peer::NotAccepting))
        setStatus(QStringLiteral("failed"), tr("对方关闭了“允许别人连接本机”"));
    else if (reason == QLatin1StringView(rc::peer::NeedsPermission))
        setStatus(QStringLiteral("failed"), tr("对方电脑还没授予屏幕录制权限，暂时不能被连接"));
    else if (reason == QLatin1StringView(rc::peer::TransportBusy) && relay_)
        setStatus(QStringLiteral("failed"), tr("对方正被别人连着，暂时没法走中继，稍后再试"));
    else if (reason == QLatin1StringView(rc::peer::TransportBusy))
        setStatus(QStringLiteral("failed"), tr("对方正被别人连着，稍后再试"));
    else if (reason == QLatin1StringView(rc::peer::NoRelay))
        setStatus(QStringLiteral("failed"), tr("连不上对方：直连打不通，对方没连上服务器，也没法走中继"));
    else if (reason == QLatin1StringView(rc::peer::NotMine))
        usePassword(tr("对方电脑没登录同一个账号，输入临时密码连接"));
    else if (reason == QLatin1StringView(rc::peer::NotReady))
        usePassword(tr("对方电脑还没连上服务器，认不出本机，输入临时密码连接"));
    else if (reason == QLatin1StringView(rc::peer::VersionMismatch))
        versionMismatch();
    else
        setStatus(QStringLiteral("failed"), tr("对方电脑出错了，暂时不能被连接"));
}

void ViewerSession::versionMismatch()
{
    setStatus(QStringLiteral("failed"),
              protocol::UpdateState::fromJson(service_.topic(protocol::UpdateTopic)).available.isEmpty()
                  ? tr("两边的比邻版本不一样，连不上。请对方更新到最新版本再连")
                  : tr("两边的比邻版本不一样，连不上。本机有新版本，更新以后再连"));
}

void ViewerSession::stateEvent(rcs_state state)
{
    switch (state) {
    case RCS_STATE_CONNECTING:
        setStatus(connectingStatus());
        break;
    case RCS_STATE_RUNNING:
        // 对方重启、注销完又连上了
        powerWait_ = {};
        retryTimer_.stop();
        setStatus(QStringLiteral("connected"));
        tellLowLatency();
        lastFrames_ = session_->stats<rcs_video_receive_stats>().decoded_frames;
        lastBytes_ = session_->stats<rcs_network_stats>().bytes;
        refreshStats();
        statsTimer_.start();
        break;
    case RCS_STATE_RECONNECTING:
        if (!power_->leaving().isEmpty() && powerWait_.action.isEmpty())
            afterPower();
        else
            setStatus(powerWait_.action.isEmpty() ? QStringLiteral("reconnecting") : connectingStatus());
        break;
    case RCS_STATE_FAILED: {
        const rcsdk::SessionFailure failure = session_->failure();
        const QString message = QString::fromStdString(failure.message);
        qInfo("连接 %s 结束：%s", qUtf8Printable(target_), qUtf8Printable(message));
        if (handledByPower())
            break;
        // 直连打不通（连通检查没找到路）：有中继就经中继再连一次
        if (failure.result == RCS_ERROR_NETWORK && !relay_ && status_ == u"connecting" && iceServers().turn) {
            qInfo("直连 %s 打不通，改走中继", qUtf8Printable(target_));
            open(true);
            break;
        }
        stop();
        if (failure.result == RCS_ERROR_AUTHENTICATION && message.contains(u"TURN"))
            setStatus(QStringLiteral("failed"), tr("中继不认本机的凭据，过一会儿再试"));
        // 按我的设备连，对方不认本机的设备密钥（它还没从服务器知道本机），或者本机不认它的（它换了设备密钥）
        else if (failure.result == RCS_ERROR_AUTHENTICATION && access_ == rc::DeviceAccess)
            usePassword(tr("对方不认这台设备，输入临时密码连接"));
        else if (failure.result == RCS_ERROR_AUTHENTICATION && message.contains(u"no more PIN attempts"))
            setStatus(QStringLiteral("needPin"), tr("试错太多次，对方的临时密码已经换了，请问对方要新的"));
        else if (failure.result == RCS_ERROR_AUTHENTICATION && message.contains(u"PIN"))
            setStatus(QStringLiteral("needPin"), tr("临时密码不对"));
        else if (failure.result == RCS_ERROR_AUTHENTICATION)
            setStatus(QStringLiteral("failed"), tr("和对方的验证没通过"));
        // 两端的网络协议不一样（SDK 握手时核对的，rc/transport.h）
        else if (failure.result == RCS_ERROR_UNSUPPORTED && message.contains(u"protocol version"))
            versionMismatch();
        else if (failure.result == RCS_ERROR_NETWORK && relay_ && status_ == u"connecting")
            setStatus(QStringLiteral("failed"), tr("连不上对方：直连打不通，中继也连不上"));
        else if (failure.result == RCS_ERROR_NETWORK)
            setStatus(QStringLiteral("failed"), tr("连不上对方，网络不通"));
        else if (failure.result == RCS_ERROR_DISCONNECTED || failure.result == RCS_ERROR_RESTARTING)
            setStatus(QStringLiteral("failed"), tr("对方断开了连接"));
        else
            setStatus(QStringLiteral("failed"), message);
        break;
    }
    default:
        break;
    }
}

// 密码对了以后等对方的人决定。没被允许时这次连接到此为止（随后的 FAILED 不再处理）
void ViewerSession::admissionEvent(rcs_admission_state state, rcs_result result, const QString &message)
{
    if (state == RCS_ADMISSION_WAITING) {
        setStatus(QStringLiteral("waiting"));
        return;
    }
    if (state != RCS_ADMISSION_REFUSED)
        return;
    qInfo("连接 %s 没被允许：%s", qUtf8Printable(target_), qUtf8Printable(message));
    stop();
    if (result == RCS_ERROR_TIMEOUT)
        setStatus(QStringLiteral("failed"), tr("对方一直没有允许，连接取消了"));
    else if (message == QLatin1StringView(rc::peer::Unattended))
        setStatus(QStringLiteral("failed"), tr("对方电脑前没有人能允许连接"));
    else
        setStatus(QStringLiteral("failed"), tr("对方拒绝了连接"));
}

void ViewerSession::requestPower(const QString &action)
{
    if (status_ == u"connected")
        power_->ask(action, *session_);
}

QString ViewerSession::connectingStatus() const
{
    if (powerWait_.action == rc::power::Restart)
        return QStringLiteral("restarting");
    if (powerWait_.action == rc::power::SignOut)
        return QStringLiteral("signingOut");
    if (powerWait_.action == Waking)
        return QStringLiteral("waking");
    return QStringLiteral("connecting");
}

bool ViewerSession::peerOnline() const
{
    const auto account = protocol::AccountState::fromJson(service_.topic(protocol::AccountTopic));
    return std::any_of(account.devices.cbegin(), account.devices.cend(), [this](const protocol::AccountDevice &device) {
        return device.deviceId == deviceId_ && device.online;
    });
}

// 重启、注销：不等 SDK 自己重连（它只等 30 秒，对方那时多半还没回来），停下来等对方回来再连；关机、睡眠：停下
void ViewerSession::afterPower()
{
    const QString action = power_->leaving();
    qInfo("%s 开始%s，连接断了", qUtf8Printable(target_), qUtf8Printable(action));
    stop();
    power_->reset();
    if (action == rc::power::Restart || action == rc::power::SignOut) {
        powerWait_ = {action, QDeadlineTimer(action == rc::power::Restart ? RestartWaitMs : SignOutWaitMs)};
        peerWasOnline_ = peerOnline();
        waitForPeer();
        return;
    }
    setStatus(action == rc::power::ShutDown ? QStringLiteral("shutDown") : QStringLiteral("asleep"));
}

bool ViewerSession::handledByPower()
{
    if (!power_->leaving().isEmpty() && powerWait_.action.isEmpty()) {
        afterPower();
        return true;
    }
    if (powerWait_.action.isEmpty())
        return false;
    waitForPeer();
    return true;
}

// 等对方回来：账号里它在线（还没来得及下线，或者回来了却还没准备好）就隔一会儿再连，不在线就等它上线（账号主题）
void ViewerSession::waitForPeer()
{
    stop();
    if (powerWait_.until.hasExpired()) {
        const QString action = powerWait_.action;
        powerWait_ = {};
        setStatus(QStringLiteral("failed"),
                  action == rc::power::Restart ? tr("对方电脑重启以后一直没回来，它开机了吗？")
                  : action == Waking           ? tr("叫了以后它一直没上线：它的网卡可能不让唤醒，或者断电了")
                                               : tr("对方注销以后一直没连上"));
        return;
    }
    setStatus(connectingStatus());
    // 不在线时到点再试最后一次（连不上就说没回来）
    retryTimer_.start(peerOnline() ? PowerRetryMs : int(powerWait_.until.remainingTime()));
}

QString ViewerSession::wakeProblem() const
{
    return DeviceWake::instance()->problem(deviceId_);
}

void ViewerSession::wake()
{
    if (!DeviceWake::instance()->wake(deviceId_))
        return;
    powerWait_ = {Waking, QDeadlineTimer(WakeWaitMs)};
    peerWasOnline_ = peerOnline();
    waitForPeer();
}

void ViewerSession::setStatus(const QString &status, const QString &reason)
{
    if (status == status_ && reason == reason_)
        return;
    status_ = status;
    reason_ = reason;
    emit stateChanged();
}

void ViewerSession::refreshStats()
{
    const auto received = session_->stats<rcs_video_receive_stats>();
    const auto network = session_->stats<rcs_network_stats>();
    const rcs_stream_info stream = session_->streamInfo();
    framesPerSecond_ = int(received.decoded_frames - lastFrames_);
    lastFrames_ = received.decoded_frames;
    latencyMs_ = int(network.round_trip_ns / 1000000);
    connectionType_ = connectionTypeName(rc::networkPath(QString::fromUtf8(network.path)));
    // 每秒刷新一次
    bitrateMbps_ = double(network.bytes - lastBytes_) * 8 / 1e6;
    lastBytes_ = network.bytes;
    resolution_ = stream.width > 0 ? QStringLiteral("%1 × %2").arg(stream.width).arg(stream.height) : QString();
    codec_ = stream.format == RCS_PICTURE_444_8 ? tr("%1 4:4:4").arg(codecName(stream.codec)) : codecName(stream.codec);
    emit statsChanged();
}

void ViewerSession::notifyFrame()
{
    if (frameQueued_.exchange(true))
        return;
    QMetaObject::invokeMethod(this, [this] {
        frameQueued_ = false;
        emit frameReady();
    }, Qt::QueuedConnection);
}

rcs_stream_info ViewerSession::streamInfo() const
{
    return session_ ? session_->streamInfo() : rcs_stream_info_init();
}

rcs_platform ViewerSession::peerPlatform() const
{
    return session_ ? session_->peerInfo().platform : RCS_PLATFORM_UNKNOWN;
}

uint32_t ViewerSession::peerInputTypes() const
{
    return session_ ? session_->peerInfo().input_types : 0;
}

const ViewerSession::CursorShape *ViewerSession::cursorShape(uint32_t id) const
{
    const auto found = cursorShapes_.constFind(id);
    return found == cursorShapes_.cend() ? nullptr : &*found;
}

rcs_result ViewerSession::sendInput(const rcs_input_event &event)
{
    return session_ ? session_->sendInput(event) : RCS_ERROR_INVALID_STATE;
}

rc::peer::SendResult ViewerSession::sendData(std::uint32_t channel, const QByteArray &data)
{
    if (!session_)
        return rc::peer::SendResult::NotConnected;
    const rcs_result result = session_->sendData(channel, data.constData(), uint32_t(data.size()));
    switch (result) {
    case RCS_OK:
        return rc::peer::SendResult::Sent;
    case RCS_ERROR_QUEUE_FULL:
        return rc::peer::SendResult::QueueFull;
    default:
        // 两次连接之间（正在重连）、已经断开（INVALID_STATE）
        qInfo("发给 %s：%s", qUtf8Printable(target_), rcs_result_message(result));
        return rc::peer::SendResult::NotConnected;
    }
}

void ViewerSession::sendControl(const QByteArray &message)
{
    if (sendData(rc::peer::ControlChannel, message) == rc::peer::SendResult::QueueFull)
        qInfo("发给 %s 的控制消息没发出去：排着的太多了", qUtf8Printable(target_));
}

void ViewerSession::requestQuality()
{
    rcs_view_request_config request = request_;
    const bool clearText = clearText_ && clearTextAvailable();
    view_quality::apply(resolutionChoice_, frameRateChoice_, bitrateChoice_,
                        view_quality::codecFor(codecChoice_, codecChoices()), clearText ? RCS_PICTURE_444_8 : 0u,
                        display_, request);
    if (request.width == request_.width && request.height == request_.height
        && request.frames_per_second == request_.frames_per_second && request.bitrate_bps == request_.bitrate_bps
        && request.codec == request_.codec && request.format == request_.format)
        return;
    if (session_->update(request) != RCS_OK) {
        qWarning("改画质：%s", rcsdk::lastCallError().c_str());
        return;
    }
    qInfo("向 %s 要画面：%s%s %u×%u %u FPS %u Mbps", qUtf8Printable(target_), qUtf8Printable(codecName(request.codec)),
          request.format == RCS_PICTURE_444_8 ? " 4:4:4" : "", request.width, request.height, request.frames_per_second,
          request.bitrate_bps / 1'000'000);
    request_ = request;
}

QList<view_quality::Codec> ViewerSession::codecs() const
{
    if (!session_ || !peerHardware_)
        return {};
    // 对方提供的哪几种带着 4:4:4（它的编码器做得了）
    const rcs_peer_info peer = session_->peerInfo();
    uint32_t peerClearText = 0;
    for (const rcs_codec_limit &limit : peer.codec_limits) {
        if (limit.codec != 0 && (limit.picture_formats & (1u << RCS_PICTURE_444_8)) != 0)
            peerClearText |= 1u << limit.codec;
    }
    const LocalCodecs::Decoders local = codecs_.decoders();
    return view_quality::codecs(peer.codecs, *peerHardware_, local.hardware, local.software,
                                peerClearText & local.clearText);
}

QList<view_quality::Codec> ViewerSession::codecChoices() const
{
    const QList<view_quality::Codec> all = codecs();
    const QList<view_quality::Codec> clear = view_quality::clearTextCodecs(all);
    return clearText_ && !clear.isEmpty() ? clear : all;
}
