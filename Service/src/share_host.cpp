#include "share_host.h"

#include "device_store.h"
#include "local_displays.h"

#include <rc/control_protocol.h>
#include <rc/peer_protocol.h>
#include <rc/power_actions.h>
#include <rc/system_service.h>

#include <rcsdk/agent.hpp>
#include <rcsdk/power.h>
#include <rcsdk/rcsdk.hpp>
#include <rcsdk/tuning.hpp>

#ifdef Q_OS_MACOS
#include "agent_gate.h"
#endif

#include <QTimer>

#include <algorithm>
#include <cstring>

namespace protocol = rc::protocol;

namespace {

// 没连上的连接两分钟内没再来信令就忘掉它的路由（观看端重连时会再来，重新记上）。连着的一直留着：
// 换了网络时 SDK 还要从它发信令
constexpr int RouteMs = 2 * 60 * 1000;
// 路断了的人等它回来多久：SDK 的观看端默认重连 30 秒
constexpr int ReturnMs = 60 * 1000;
// 系统不通知授权变了：缺授权时隔一会儿看一次
constexpr int PermissionPollMs = 2000;
// 分享会话运行中出了错（编码器起不来这样的）：隔一会儿重新开，接着接受连接；接连出错时一次比一次等得久
constexpr int RestartFirstMs = 2000;
constexpr int RestartMaxMs = 60 * 1000;
// 电源模块答应以后等多久才做：回答、事件先发出去（模块最少 1 秒）
constexpr uint32_t PowerDelayMs = 1000;

const char *stateName(rcs_state state)
{
    switch (state) {
    case RCS_STATE_STARTING: return "starting";
    case RCS_STATE_LISTENING: return "listening";
    case RCS_STATE_RUNNING: return "running";
    case RCS_STATE_STOPPED: return "stopped";
    case RCS_STATE_FAILED: return "failed";
    default: return "?";
    }
}

QString platformName(rcs_platform platform)
{
    switch (platform) {
    case RCS_PLATFORM_MACOS: return QStringLiteral("macOS");
    case RCS_PLATFORM_WINDOWS: return QStringLiteral("Windows");
    case RCS_PLATFORM_IOS: return QStringLiteral("iOS");
    case RCS_PLATFORM_ANDROID: return QStringLiteral("Android");
    default: return {};
    }
}

// 给一个人录声音的情况（SDK 的 rcs_audio_event，发的方向）：录得了、对方不要声音时是空的
QLatin1StringView soundProblem(const rcs_audio_event &audio)
{
    if (audio.active || audio.result == RCS_OK)
        return {};
    if (audio.result == RCS_ERROR_DEVICE)
        return rc::control::SoundNoDevice;
    if (audio.result == RCS_ERROR_PERMISSION_REQUIRED)
        return rc::control::SoundNoPermission;
    return rc::control::SoundFailed;
}

template<typename List>
auto findId(List &peers, int id)
{
    return std::find_if(peers.begin(), peers.end(), [id](const auto &peer) { return peer.info.id == id; });
}

// SDK 的定长密钥（rcsdk::DeviceKey、rcsdk::PublicKey），bytes 的长度和它一样
template<typename Key>
Key keyFrom(const QByteArray &bytes)
{
    Key key;
    std::copy(bytes.cbegin(), bytes.cend(), key.begin());
    return key;
}

} // namespace

ShareHost::ShareHost(QLatin1StringView access, const ShareCapture &capture, DeviceStore &store,
                     rc::InterruptedTransfers &transfers, QObject *parent)
    : QObject(parent)
    , access_(access)
    , capture_(capture)
    , store_(store)
    , displays_([this](quint32 id) { return showDisplay(id); },
                [this](int viewer, const QByteArray &message) { sendControl(viewer, message); },
                [this](int viewer) {
                    const auto found = findId(viewers_, viewer);
                    return found != viewers_.end() && found->info.control;
                })
    , files_(transfers)
    , thumbnails_([this](int viewer, const QByteArray &message) { sendControl(viewer, message); })
{
    channels_.add(rc::peer::ControlChannel,
                  [this](int viewer, const QByteArray &message) { receivedControl(viewer, message); });
#ifdef Q_OS_MACOS
    // 来连的助手的授权（认没认它）搬到这个对象的线程
    if (capture_.where == ShareCapture::Where::Agents) {
        agentGate_ = consoleAgentGate([this](const rcs_agent_info &agent) {
            QMetaObject::invokeMethod(this, [this, agent] { agentChanged(agent); }, Qt::QueuedConnection);
        });
    }
#endif
    permissionTimer_.setInterval(PermissionPollMs);
    connect(&permissionTimer_, &QTimer::timeout, this, &ShareHost::checkPermissions);
    restartDelayMs_ = RestartFirstMs;
    restartTimer_.setSingleShot(true);
    connect(&restartTimer_, &QTimer::timeout, this, [this] {
        problem_.clear();
        update();
    });
    readPermissions();
    // 本进程里编码时用显卡编得了哪几种；助手编码时是助手的（agentChanged）
    if (capture_.where != ShareCapture::Where::Agents) {
        rcs_capabilities capabilities = rcs_capabilities_init();
        if (rcs_get_capabilities(&capabilities) == RCS_OK)
            hardwareEncoders_ = capabilities.hardware_encoders;
    }
    connect(&store_, &DeviceStore::changed, this, &ShareHost::update);
    update();
}

ShareHost::~ShareHost() = default;

// 我的设备：账号下有别的设备，界面程序开着、或者允许没人开界面时也连（DeviceState::unattended）
bool ShareHost::wanted() const
{
    if (!mine())
        return store_.state().acceptIncoming;
    return trustedKeys_ && !trustedKeys_->isEmpty() && (attended_ || store_.state().unattended);
}

void ShareHost::setTrustedKeys(const std::optional<QList<QByteArray>> &keys)
{
    if (keys == trustedKeys_)
        return;
    trustedKeys_ = keys;
    if (keys)
        qInfo("账号下别的设备有 %lld 台，它们不要密码就能连本机", qlonglong(keys->size()));
    if (!session_ || !wanted()) {
        update();
        return;
    }
    // 从下一个握手起只认这些，已经连着的不受影响
    rcsdk::SessionConfig config(RCS_ROLE_SHARE);
    useIdentity(config);
    if (session_->update(config) != RCS_OK)
        qWarning("换信任的设备：%s", rcsdk::lastCallError().c_str());
}

void ShareHost::update()
{
    if (!wanted()) {
        stop();
        return;
    }
    if (!session_) {
        // 没起来是因为缺授权：等授权（checkPermissions），不是每次设置变了都再试一遍
        if (problem_.isEmpty())
            start();
        return;
    }
    const auto device = store_.state();
    if (mine() || device.password == pin_)
        return;
    // 换了临时密码：从下一个握手起用新的，已经连着的不受影响
    pin_ = device.password;
    rcsdk::SessionConfig config(RCS_ROLE_SHARE);
    config.setPin(pin_.toStdString());
    if (const rcs_result result = session_->setIdentity(config.core().identity); result != RCS_OK)
        qWarning("换临时密码：%s", rcs_result_message(result));
}

void ShareHost::start()
{
    readPermissions();
    // 授权是助手的：先开着，等授了权的助手连上来
    if (capture_.where != ShareCapture::Where::Agents && !screenPermission_) {
        qInfo("还没授予屏幕录制，别人现在连不上本机");
        problem_ = QLatin1StringView(rc::peer::NeedsPermission);
        permissionTimer_.start();
        emit stateChanged();
        return;
    }
    pin_ = store_.state().password;
    // 能不能让对方控制在开分享会话时定：没有辅助功能授权时只能让对方看
    canControl_ = inputPermission_;
    const quint64 run = ++run_;
    rcsdk::SessionConfig config(RCS_ROLE_SHARE);
    useIdentity(config);
    config.setControl(canControl_);
    // 没有硬件编码器的电脑（老电脑、虚拟机）用系统自带的软件编码，不然连不上本机；不另带编码库。
    // 编码格式四种都列，本机编得了的才提供（VP9 只有 Intel 显卡编得了），连进来的人按两边都用显卡的挑
    // （rc/control_protocol.h 的 encoders），不挑时用 H.264
    config.setAcceleration(RCS_ACCELERATION_HARDWARE_FIRST);
    config.setCodecs({RCS_CODEC_H264, RCS_CODEC_HEVC, RCS_CODEC_AV1, RCS_CODEC_VP9});
    // 画面格式默认 4:2:0；观看端要文字更清楚时用 4:4:4（显卡编得了的编码格式才带着它，SDK 按编码器报）
    config.setPictureFormats({RCS_PICTURE_420_8, RCS_PICTURE_444_8});
    lowLatency_ = false;
    useTransport(config);
    useCapture(config);
    // 电源模块：有人连着时电脑不睡（模块默认就是这样）；我的设备还能让本机重启、关机、睡眠、注销，正在控制本机的
    // 那一台可以要（SDK 默认的规则），别人的电脑一律不能。去连的一方点之前还要确认，所以不倒数
    rcs_power_settings power = rcs_power_settings_init();
    power.actions = mine() ? RCS_POWER_ALL_ACTIONS : 0u;
    // 我的设备还告诉它本机能唤醒的网卡（MAC 地址认得出是哪台电脑：别人一律不告诉），它记着，本机睡眠、关机以后
    // 好叫醒（App 的 DeviceWake）
    power.share_wake_info = mine() ? 1u : 0u;
    power.action_delay_ms = PowerDelayMs;
    config.useModule(RCS_POWER_MODULE, power);
    // 观看端换显示器之前要的缩略图
    ShareThumbnails::configure(config);
    // 换网络时路断了一阵又回来的人控制权照旧，和断开后重新连上的一样（viewerArrived）
    config.extension<rcs_paths_config>().restore_control = 1;
    // 声音：分享端默认就把本机正在播放的声音发出去，对方的不收（不做麦克风）。不排除本程序的声音：后台服务自己
    // 不放声音。macOS 上用 ScreenCaptureKit 录，和屏幕录制同一个授权；SDK 默认的 Core Audio tap 要另一个授权
    // （"仅系统音频录制"），系统没有公开的接口查它授没授，没授时对方听不到，本机也没法说明为什么
#ifdef Q_OS_MACOS
    config.extension<rcs_audio_tuning_config>().capture_backend = RCS_AUDIO_CAPTURE_SCREENCAPTUREKIT;
#endif
    clipboard_.configure(config);
    rc::FileTransfers::configure(config);

    rcsdk::Callbacks callbacks;
    // SDK 的线程上：搬到这个对象的线程再处理（later）。对象没了，排着的也就不做了；
    // 分享会话停了、重新开了，上一次排着的也不做
    const auto later = [this, run](auto &&work) {
        QMetaObject::invokeMethod(this, [this, run, work = std::forward<decltype(work)>(work)] {
            if (run == run_)
                work();
        }, Qt::QueuedConnection);
    };
    callbacks.onSignal = [this, later](const std::string &text, uint64_t route) {
        later([this, route, text = QString::fromStdString(text)] { sdkAnswered(route, text); });
    };
    // 读连接的线程上：复制下来就返回。等本机的人点允许时不会有（SDK 不交换消息）
    callbacks.onData = [this, later](const rcs_data_message &message) {
        const int id = int(message.viewer_id);
        const std::uint32_t channel = message.channel;
        QByteArray data(reinterpret_cast<const char *>(message.data), qsizetype(message.size));
        later([this, id, channel, data = std::move(data)] { channels_.receive(channel, id, data); });
    };
    // 别人：密码对了先不让进，等本机的人决定（admit / refuse）。
    // 我的设备：SDK 核对过它的设备密钥在信任列表里，直接让进、能控制（没有辅助功能授权时只能看）
    const bool admit = mine();
    const bool control = canControl_;
    callbacks.onViewerJoin = [this, later, admit, control](const rcs_viewer_info &viewer, rcs_viewer_decision &decision) {
        decision.admission = admit ? RCS_ADMIT : RCS_ADMIT_LATER;
        decision.control = control;
        const int id = int(viewer.viewer_id);
        const quint64 route = viewer.route;
        const rcs_platform platform = viewer.peer.platform;
        later([this, id, route, platform, admit] { viewerArrived(id, route, platformName(platform), admit); });
    };
    callbacks.onEvent = [this, later](const rcs_event &event) {
        if (auto file = rc::FileTransfers::read(event)) {
            later([this, file = std::move(*file)] { files_.handle(file); });
            return;
        }
        const int id = int(event.viewer_id);
        switch (event.type) {
        case RCS_EVENT_STATE: {
            const rcs_state state = event.state.value;
            const rcs_result result = event.state.result;
            later([this, state, result] {
                if (state == RCS_STATE_FAILED) {
                    runFailed();
                    return;
                }
                qInfo("分享会话：%s（%s）", stateName(state), rcs_result_message(result));
                // 有人连上来了、好好的：下次出错又从 2 秒开始等
                if (state == RCS_STATE_RUNNING)
                    restartDelayMs_ = RestartFirstMs;
                // 一个连接都没有了（回绝的话已经送到）：用着中继、人都走了就换回直连
                if (state == RCS_STATE_LISTENING)
                    switchBackIfIdle();
            });
            break;
        }
        case RCS_EVENT_DISPLAYS:
            later([this] { readDisplays(); });
            break;
        case RCS_EVENT_AGENT: {
            const rcs_agent_info agent = event.agent;
            later([this, agent] { agentChanged(agent); });
            break;
        }
        case RCS_EVENT_PIN: {
            const int left = int(event.pin.attempts_left);
            later([this, left] { pinFailed(left); });
            break;
        }
        case RCS_EVENT_ADMISSION:
            // 等着的请求没被允许就结束了：一分钟没人点（SDK 拒绝），或者对方自己走了
            if (event.admission.state == RCS_ADMISSION_REFUSED || event.admission.state == RCS_ADMISSION_GONE) {
                const bool timedOut = event.admission.state == RCS_ADMISSION_REFUSED;
                later([this, id, timedOut] { requestEnded(id, timedOut); });
            }
            break;
        case RCS_EVENT_VIEWER: {
            const rcs_viewer_info &viewer = event.viewer;
            if (viewer.joined) {
                const quint64 route = viewer.route;
                const rcs_platform platform = viewer.peer.platform;
                const bool control = viewer.control.allowed != 0;
                later([this, id, route, platform, control] { viewerJoined(id, route, platformName(platform), control); });
            } else {
                // 路断了（网络），或者对方说它会回来：等它重连
                const bool pathBroke = viewer.result == RCS_ERROR_NETWORK || viewer.result == RCS_ERROR_RESTARTING;
                later([this, id, pathBroke] { viewerLeft(id, pathBroke); });
            }
            break;
        }
        case RCS_EVENT_ENCODING:
            // 运行中改画质（编码格式、尺寸、帧率），编码器打不开：这次改动没生效，画面照原来的继续
            if (event.encoding.encoding == 0 && event.encoding.running) {
                const rcs_encoding_event &encoding = event.encoding;
                qWarning("编码器不接受 %u×%u %u FPS，画面照原来的继续：%s（%s）", encoding.width, encoding.height,
                         encoding.frames_per_second, rcs_result_message(encoding.result), encoding.message);
            }
            break;
        case RCS_EVENT_AUDIO:
            if (event.audio.direction == RCS_AUDIO_SENDING) {
                const QLatin1StringView problem = soundProblem(event.audio);
                const QString message = QString::fromUtf8(event.audio.message);
                later([this, id, problem, message] { soundChanged(id, problem, message); });
            }
            break;
        case RCS_EVENT_MODULE:
            // 我的设备要的电源操作，本机答应了：马上就做，记一笔
            if (std::strcmp(event.module.module, RCS_POWER_MODULE) == 0 && event.module.kind == RCS_POWER_ACTION) {
                const auto &action = *static_cast<const rcs_power_action_event *>(event.module.payload);
                if (action.state == RCS_POWER_ACTION_ACCEPTED) {
                    const QString name = rc::power::name(action.action);
                    later([this, id, name] { powerAccepted(id, name); });
                }
            }
            break;
        default:
            break;
        }
    };

    try {
        session_ = std::make_unique<rcsdk::Session>(config, std::move(callbacks));
    } catch (const rcsdk::Error &error) {
        qCritical("分享会话建不起来：%s", error.what());
        problem_ = QLatin1StringView(rc::peer::Unavailable);
        emit stateChanged();
        return;
    }
    files_.attach(session_->handle());
    if (const rcs_result result = session_->start(); result != RCS_OK) {
        runFailed();
        return;
    }
    qInfo("开始接受%s连接本机（%s）%s", mine() ? "我的设备" : "别人",
          transport_.current() == rc::RelayTransport ? "经中继" : "直连",
          canControl_ ? "" : "，还没授予辅助功能，只能让对方看");
    problem_.clear();
    // 授权是助手的时候不用定时看本进程的
    if (inputPermission_ || capture_.where == ShareCapture::Where::Agents)
        permissionTimer_.stop();
    else
        permissionTimer_.start();
    emit stateChanged();
}

void ShareHost::stopForRestart()
{
    if (!session_)
        return;
    qInfo("后台服务要被系统重启：连着本机的人等这边回来");
    session_->stop(RCS_STOP_RESTART);
    end();
}

void ShareHost::stop()
{
    restartTimer_.stop();
    const bool wasRunning = session_ != nullptr;
    end();
    transport_.switchTo(rc::DirectTransport);
    problem_.clear();
    emit stateChanged();
    if (wasRunning)
        qInfo("不再接受%s连接本机", mine() ? "我的设备" : "别人");
}

void ShareHost::end()
{
    permissionTimer_.stop();
    // 连着的人都不在了：传输也都不要了
    files_.detach();
    files_.clear();
    thumbnails_.stop();
    session_.reset();
    ++run_;
    canControl_ = false;
    routes_.clear();
    for (const Peer &viewer : std::as_const(viewers_))
        emit viewerGone(viewer.info.deviceId, viewer.session);
    for (const Returning &returning : std::as_const(returning_))
        emit viewerGone(returning.device, returning.session);
    returning_.clear();
    rejoined_.clear();
    requests_.clear();
    viewers_.clear();
    displays_.reset();
}

void ShareHost::restart()
{
    if (!wanted())
        return;
    reopen(rc::DirectTransport);
}

void ShareHost::reopen(const QString &transport)
{
    end();
    problem_.clear();
    transport_.switchTo(transport);
    start();
}

void ShareHost::useIdentity(rcsdk::SessionConfig &config) const
{
    if (!mine()) {
        config.setPin(pin_.toStdString());
        return;
    }
    std::vector<rcsdk::PublicKey> trusted;
    for (const QByteArray &key : *trustedKeys_)
        trusted.push_back(keyFrom<rcsdk::PublicKey>(key));
    config.setSecret(RCS_SECRET_NONE)
        .setDeviceKey(keyFrom<rcsdk::DeviceKey>(store_.deviceKey()))
        .setTrustedKeys(std::move(trusted));
}

void ShareHost::useCapture(rcsdk::SessionConfig &config)
{
    switch (capture_.where) {
    case ShareCapture::Where::Here:
        break;
    case ShareCapture::Where::SecureDesktops:
        config.setSecureDesktop(true);
        break;
    case ShareCapture::Where::Agents: {
#ifdef Q_OS_MACOS
        rcsdk::ConnectionKey key{};
        std::copy(capture_.agentKey.cbegin(), capture_.agentKey.cend(), key.begin());
        // 助手以登录的那个用户运行：每个账户都能打开链接，认哪个由 agentGate_ 定
        config.use(rcsdk::agentLink(rc::system_service::agentEndpoint(access_).toStdString(), key,
                                    RCS_AGENT_ACCESS_ANY_USER, agentGate_.get()));
#endif
        break;
    }
    }
}

void ShareHost::useTransport(rcsdk::SessionConfig &config) const
{
    if (transport_.current() == rc::RelayTransport) {
        const rc::TurnServer &turn = *ice_.turn;
        config.setRelay(turn.server.toStdString(), turn.username.toStdString(), turn.password.toStdString(),
                        turn.tcp ? RCS_TURN_TCP : RCS_TURN_UDP);
    } else {
        config.setTransport(RCS_TRANSPORT_P2P).setStunServers(ice_.stunServers());
    }
}

void ShareHost::setIceServers(const rc::IceServers &ice)
{
    ice_ = ice;
    // 用着中继、中继却没了（凭据过期了又连不上服务器）：连着的人照旧，新来要中继的回绝，人走了换回直连
    if (!session_ || (transport_.current() == rc::RelayTransport && !ice_.turn))
        return;
    rcsdk::SessionConfig config(RCS_ROLE_SHARE);
    useTransport(config);
    if (session_->update(config.iceServers()) != RCS_OK)
        qWarning("换 P2P 和中继用的服务器：%s", rcsdk::lastCallError().c_str());
}

bool ShareHost::inUse() const
{
    return !requests_.isEmpty() || !viewers_.isEmpty() || !returning_.isEmpty();
}

void ShareHost::switchBackIfIdle()
{
    if (!transport_.shouldSwitchBack(inUse()))
        return;
    qInfo("经中继连本机的人都走了，换回直连");
    reopen(rc::DirectTransport);
}

// 启动不了，或者运行中出了错（比如屏幕录制授权被收回）：别人连不上了，说明原因
void ShareHost::runFailed()
{
    const rcsdk::SessionFailure failure = session_->failure();
    qCritical("分享会话停了：%s", failure.message.c_str());
    end();
    // 经中继时停了（中继连不上、凭据被拒……）：换回直连接着接受连接
    if (transport_.current() == rc::RelayTransport) {
        transport_.switchTo(rc::DirectTransport);
        start();
        return;
    }
    if (failure.result == RCS_ERROR_PERMISSION_REQUIRED) {
        problem_ = QLatin1StringView(rc::peer::NeedsPermission);
        permissionTimer_.start();
    } else {
        // 等的时候来连的人被回绝 unavailable，过一会儿再连就行
        problem_ = QLatin1StringView(rc::peer::Unavailable);
        qInfo("%d 秒后重新开分享会话", restartDelayMs_ / 1000);
        restartTimer_.start(restartDelayMs_);
        restartDelayMs_ = std::min(restartDelayMs_ * 2, RestartMaxMs);
    }
    emit stateChanged();
}

// 本进程的授权；助手做采集的时候是助手的（agentChanged）
void ShareHost::readPermissions()
{
    if (capture_.where == ShareCapture::Where::Agents)
        return;
    screenPermission_ = rcsdk::screenCapturePermission() == RCS_PERMISSION_GRANTED;
    inputPermission_ = rcsdk::inputPermission() == RCS_PERMISSION_GRANTED;
}

void ShareHost::agentChanged(const rcs_agent_info &agent)
{
    // 换了助手（换了用户、到了登录界面）：用显卡编得了的跟着变，告诉连着的人
    if (agent.attached && agent.hardware_encoders != hardwareEncoders_) {
        hardwareEncoders_ = agent.hardware_encoders;
        sendControl(0, rc::control::encoders(hardwareEncoders_));
    }
    const bool screen = agent.screen_permission == RCS_PERMISSION_GRANTED;
    const bool input = agent.input_permission == RCS_PERMISSION_GRANTED;
    const bool changed = screen != screenPermission_ || input != inputPermission_;
    screenPermission_ = screen;
    inputPermission_ = input;
    permissionsChanged(changed);
}

void ShareHost::powerAccepted(int id, const QString &action)
{
    const auto viewer = findId(viewers_, id);
    if (viewer == viewers_.end())
        return;
    qInfo("%s（%s）让本机%s", qUtf8Printable(viewer->info.name), qUtf8Printable(viewer->info.deviceId), qUtf8Printable(action));
    emit powerDone(viewer->info, action);
}

// 缺授权时定时来看：授了屏幕录制就开始接受连接；辅助功能授了（或者收回了），没人连着时按它重开分享会话
void ShareHost::checkPermissions()
{
    const bool screen = screenPermission_;
    const bool input = inputPermission_;
    readPermissions();
    permissionsChanged(screen != screenPermission_ || input != inputPermission_);
}

void ShareHost::permissionsChanged(bool changed)
{
    if (changed)
        qInfo("授权变了：屏幕录制%s，辅助功能%s", screenPermission_ ? "有" : "没有", inputPermission_ ? "有" : "没有");
    if (!session_) {
        if (screenPermission_ && wanted())
            start();
        else if (changed)
            emit stateChanged();
        return;
    }
    if (canControl_ != inputPermission_ && requests_.isEmpty() && viewers_.isEmpty()) {
        restart();
        return;
    }
    if (changed)
        emit stateChanged();
}

void ShareHost::requestPermission(const QString &permission)
{
    // 助手自己启动时要（授权提示要由要授权的那个程序弹）
    if (capture_.where == ShareCapture::Where::Agents)
        return;
    if (permission == protocol::ScreenPermission && !screenPermission_)
        rcsdk::requestScreenCapturePermission();
    else if (permission == protocol::InputPermission && !inputPermission_)
        rcsdk::requestInputPermission();
}

void ShareHost::setAttended(bool attended)
{
    attended_ = attended;
    files_.setAttended(attended);
    // 我的设备不允许没人开界面时连：随界面开始、结束
    if (mine())
        update();
    if (attended_ || requests_.isEmpty())
        return;
    // 界面程序没了：等着的请求没人能点了
    for (const Peer &request : std::as_const(requests_))
        decide(request.info.id, false, false, rc::peer::Unattended);
    requests_.clear();
    emit stateChanged();
    changePassword("界面程序关了，等着的请求都拒绝了");
}

protocol::ShareState ShareHost::state() const
{
    protocol::ShareState state;
    state.accepting = session_ != nullptr;
    state.problem = problem_;
    state.screenPermission = screenPermission_;
    state.inputPermission = inputPermission_;
    state.canControl = canControl_;
    state.clipboard = clipboard_.enabled();
    for (const Peer &request : requests_)
        state.requests.append(request.info);
    for (const Peer &viewer : viewers_)
        state.viewers.append(viewer.info);
    return state;
}

void ShareHost::receiveSignal(const QString &from, const QString &name, const QString &session, const QString &text,
                              const rc::ConnectionMode &mode)
{
    // 现在不接受连接：经服务器告诉对方为什么，不让它干等
    if (!session_) {
        QString reason = problem_;
        if (reason.isEmpty() && !mine())
            reason = QLatin1StringView(rc::peer::NotAccepting);
        else if (reason.isEmpty())
            reason = QLatin1StringView(trustedKeys_ ? rc::peer::NotMine : rc::peer::NotReady);
        qInfo("设备 %s 要连本机，本机现在不接受连接（%s），回绝", qUtf8Printable(from), qUtf8Printable(reason));
        emit declined(from, session, reason);
        return;
    }
    switch (transport_.check(from, mode.transport, inUse(), ice_.turn.has_value())) {
    case ShareTransport::Verdict::Accept:
        break;
    case ShareTransport::Verdict::Switch:
        qInfo("设备 %s 要%s连本机，换过去", qUtf8Printable(from), mode.transport == rc::RelayTransport ? "经中继" : "直连");
        reopen(mode.transport);
        // 起不来（缺授权、出错了；中继出了问题时换回了直连）
        if (!session_ || transport_.current() != mode.transport) {
            emit declined(from, session, problem_.isEmpty() ? QLatin1StringView(rc::peer::Unavailable) : problem_);
            return;
        }
        break;
    case ShareTransport::Verdict::Busy:
        qInfo("设备 %s 要%s连本机，本机正被别人用另一种方式连着，回绝", qUtf8Printable(from),
              mode.transport == rc::RelayTransport ? "经中继" : "直连");
        emit declined(from, session, QLatin1StringView(rc::peer::TransportBusy));
        return;
    case ShareTransport::Verdict::NoRelay:
        qInfo("设备 %s 要经中继连本机，本机没有中继（没连上服务器），回绝", qUtf8Printable(from));
        emit declined(from, session, QLatin1StringView(rc::peer::NoRelay));
        return;
    }
    transport_.attempting(from);
    if (const rcs_result result = session_->signal(text.toStdString(), routeFor(from, name, session)); result != RCS_OK)
        qWarning("设备 %s 的信令：%s", qUtf8Printable(from), rcs_result_message(result));
}

void ShareHost::admit(int id, bool control)
{
    const auto request = findId(requests_, id);
    if (request == requests_.end())
        return;
    qInfo("允许 %s（%s）连接本机，%s", qUtf8Printable(request->info.name), qUtf8Printable(request->info.deviceId),
          control ? "可以控制" : "仅查看");
    requests_.erase(request);
    emit stateChanged();
    decide(id, true, control);
}

void ShareHost::refuse(int id)
{
    const auto request = findId(requests_, id);
    if (request == requests_.end())
        return;
    qInfo("拒绝 %s（%s）连接本机", qUtf8Printable(request->info.name), qUtf8Printable(request->info.deviceId));
    emit requestFailed(request->info, QStringLiteral("refused"));
    requests_.erase(request);
    emit stateChanged();
    decide(id, false, false, rc::peer::RefusedByUser);
    changePassword("拒绝了请求");
}

void ShareHost::disconnectViewer(int id)
{
    // 它走了以后（viewerLeft）再从列表里去掉
    if (findId(viewers_, id) != viewers_.end())
        session_->disconnectViewer(rcs_viewer_id(id));
}

void ShareHost::disconnectAll()
{
    // 它们走了以后（viewerLeft）再从列表里去掉、换临时密码
    for (const Peer &viewer : std::as_const(viewers_))
        session_->disconnectViewer(rcs_viewer_id(viewer.info.id));
    if (!returning_.isEmpty()) {
        returning_.clear();
        changePassword("断开的人不再等它回来");
    }
}

void ShareHost::setControl(int id, bool control)
{
    const auto viewer = findId(viewers_, id);
    if (viewer == viewers_.end() || viewer->info.control == control)
        return;
    if (const rcs_result result = session_->setViewerControl(rcs_viewer_id(id), control); result != RCS_OK) {
        qWarning("改 %s 的控制权：%s", qUtf8Printable(viewer->info.name), rcsdk::lastCallError().c_str());
        return;
    }
    viewer->info.control = control;
    emit stateChanged();
}

void ShareHost::setClipboard(bool clipboard)
{
    if (!clipboard_.setEnabled(clipboard, session_.get()))
        return;
    qInfo("%s和连着本机的人同步剪贴板", clipboard ? "开始" : "停止");
    emit stateChanged();
}

void ShareHost::addChannel(std::uint32_t channel, rc::peer::DataChannels::Handler handler)
{
    channels_.add(channel, std::move(handler));
}

rc::peer::SendResult ShareHost::sendData(int id, std::uint32_t channel, const QByteArray &data)
{
    if (!session_)
        return rc::peer::SendResult::NotConnected;
    const rcs_result result = session_->sendDataTo(rcs_viewer_id(id), channel, data.constData(), uint32_t(data.size()));
    switch (result) {
    case RCS_OK:
        return rc::peer::SendResult::Sent;
    case RCS_ERROR_QUEUE_FULL:
        return rc::peer::SendResult::QueueFull;
    default:
        // 对方走了、正在重连（INVALID_STATE），或者是分享会话重开以前的人（INVALID_ARGUMENT）
        qInfo("发给连着本机的人 %d：%s", id, rcs_result_message(result));
        return rc::peer::SendResult::NotConnected;
    }
}

quint64 ShareHost::routeFor(const QString &from, const QString &name, const QString &session)
{
    routes_.removeIf([this](const auto &entry) { return entry.value().expires.hasExpired() && !inUse(entry.value()); });
    for (auto it = routes_.begin(); it != routes_.end(); ++it) {
        if (it->device == from && it->session == session) {
            it->name = name;
            it->expires = QDeadlineTimer(RouteMs);
            return it.key();
        }
    }
    const quint64 route = nextRoute_++;
    routes_.insert(route, {from, name, session, QDeadlineTimer(RouteMs)});
    return route;
}

bool ShareHost::inUse(const Route &route) const
{
    const auto same = [&route](const QString &device, const QString &session) {
        return device == route.device && session == route.session;
    };
    return std::any_of(viewers_.cbegin(), viewers_.cend(), [&same](const Peer &p) { return same(p.info.deviceId, p.session); })
        || std::any_of(requests_.cbegin(), requests_.cend(), [&same](const Peer &p) { return same(p.info.deviceId, p.session); })
        || std::any_of(returning_.cbegin(), returning_.cend(), [&same](const Returning &r) { return same(r.device, r.session); });
}

void ShareHost::sdkAnswered(quint64 route, const QString &text)
{
    const auto it = routes_.constFind(route);
    if (it == routes_.cend())
        return;
    emit answer(it->device, it->session, text);
}

void ShareHost::pinFailed(int attemptsLeft)
{
    qWarning("有人输错了临时密码，还能试 %d 次", attemptsLeft);
    emit wrongPin();
    if (attemptsLeft == 0)
        changePassword("试错太多次");
}

ShareHost::Peer ShareHost::peerFor(int id, quint64 route, const QString &platform) const
{
    const Route from = routes_.value(route);
    return {{id, from.device, from.name, platform, false}, from.session};
}

void ShareHost::viewerArrived(int id, quint64 route, const QString &platform, bool admitted)
{
    Peer peer = peerFor(id, route, platform);
    transport_.arrived(peer.info.deviceId);
    // 路断了的人回来了：直接让它进，控制权照旧
    const auto returning = std::find_if(returning_.begin(), returning_.end(), [&peer](const Returning &r) {
        return r.device == peer.info.deviceId && r.session == peer.session;
    });
    if (returning != returning_.end()) {
        qInfo("%s（%s）重新连上了", qUtf8Printable(peer.info.name), qUtf8Printable(peer.info.deviceId));
        const bool control = returning->control;
        returning_.erase(returning);
        rejoined_.insert(id);
        if (!admitted)
            decide(id, true, control);
        return;
    }
    // 我的设备：已经让进了，连进来时（viewerJoined）记上
    if (admitted)
        return;
    if (!attended_) {
        qInfo("%s（%s）要连本机，界面程序没开着，没人能点允许，拒绝", qUtf8Printable(peer.info.name),
              qUtf8Printable(peer.info.deviceId));
        emit requestFailed(peer.info, QStringLiteral("unattended"));
        decide(id, false, false, rc::peer::Unattended);
        changePassword("没人能点允许，拒绝了请求");
        return;
    }
    qInfo("%s（%s）请求连接本机，等本机的人允许", qUtf8Printable(peer.info.name), qUtf8Printable(peer.info.deviceId));
    requests_.append(std::move(peer));
    emit stateChanged();
}

void ShareHost::requestEnded(int id, bool timedOut)
{
    const auto request = findId(requests_, id);
    if (request == requests_.end())
        return;
    qInfo("%s（%s）的请求结束了：%s", qUtf8Printable(request->info.name), qUtf8Printable(request->info.deviceId),
          timedOut ? "一直没人点" : "对方走了");
    if (timedOut)
        emit requestFailed(request->info, QStringLiteral("timeout"));
    requests_.erase(request);
    emit stateChanged();
    changePassword("请求没被允许");
}

void ShareHost::viewerJoined(int id, quint64 route, const QString &platform, bool control)
{
    Peer viewer = peerFor(id, route, platform);
    viewer.info.control = control;
    qInfo("%s（%s）连进来了", qUtf8Printable(viewer.info.name), qUtf8Printable(viewer.info.deviceId));
    // 显示器变了的话先告诉已经连着的人，再告诉它有哪几块
    readDisplays();
    // 路断了回来的还是原来那一次连接
    if (!rejoined_.remove(id))
        emit viewerConnected(viewer.info, viewer.session);
    // 和它上次没传完的文件：按设备认（按 IP 连进来的也有设备 ID）
    files_.setPeer(id, viewer.info.deviceId);
    viewers_.append(std::move(viewer));
    emit stateChanged();
    displays_.viewerJoined(id);
    tellMainViewer();
    sendControl(id, rc::control::encoders(hardwareEncoders_));
    if (!wallpaper_.isEmpty())
        sendControl(id, rc::control::wallpaper(wallpaper_));
    emit wallpaperWanted();
}

void ShareHost::setWallpaper(const QByteArray &jpeg)
{
    if (jpeg == wallpaper_)
        return;
    wallpaper_ = jpeg;
    sendControl(0, rc::control::wallpaper(wallpaper_));
}

void ShareHost::viewerLeft(int id, bool pathBroke)
{
    const auto viewer = findId(viewers_, id);
    if (viewer == viewers_.end())
        return;
    const Peer left = *viewer;
    viewers_.erase(viewer);
    emit stateChanged();
    tellMainViewer();
    applyLowLatency();
    if (!pathBroke) {
        qInfo("%s（%s）断开了", qUtf8Printable(left.info.name), qUtf8Printable(left.info.deviceId));
        emit viewerGone(left.info.deviceId, left.session);
        changePassword("连进来的人走了");
        showMainIfNobodyWatches();
        return;
    }
    qInfo("和 %s（%s）的路断了，等它回来", qUtf8Printable(left.info.name), qUtf8Printable(left.info.deviceId));
    returning_.append({left.info.deviceId, left.session, left.info.control, QDeadlineTimer(ReturnMs)});
    QTimer::singleShot(ReturnMs, this, &ShareHost::forgetReturning);
}

void ShareHost::soundChanged(int id, QLatin1StringView problem, const QString &message)
{
    const auto viewer = findId(viewers_, id);
    if (viewer == viewers_.end() || viewer->sound == problem)
        return;
    viewer->sound = problem;
    if (problem.isEmpty())
        qInfo("又能给 %s 录声音了", qUtf8Printable(viewer->info.name));
    else
        qWarning("给 %s 录不了声音：%s", qUtf8Printable(viewer->info.name), qUtf8Printable(message));
    sendControl(id, rc::control::sound(problem));
}

void ShareHost::forgetReturning()
{
    const qsizetype gone = returning_.removeIf([this](const Returning &r) {
        if (!r.expires.hasExpired())
            return false;
        emit viewerGone(r.device, r.session);
        return true;
    });
    if (gone == 0)
        return;
    changePassword("断开的人没回来");
    showMainIfNobodyWatches();
    switchBackIfIdle();
}

void ShareHost::decide(int id, bool admit, bool control, const char *reason)
{
    rcs_viewer_decision decision = rcs_viewer_decision_init();
    decision.admission = admit ? RCS_ADMIT : RCS_REFUSE;
    decision.control = control;
    qstrncpy(decision.reason, reason, sizeof decision.reason);
    // 对方可能刚好走了
    if (session_->admitViewer(rcs_viewer_id(id), decision) != RCS_OK)
        qInfo("请求 %d 已经不在等了：%s", id, rcsdk::lastCallError().c_str());
}

void ShareHost::changePassword(const char *why)
{
    // 我的设备不用临时密码
    if (mine())
        return;
    qInfo("临时密码用过了（%s），换一个新的", why);
    store_.regeneratePassword();
}

// 画面听先连上的人的（SDK 的主观看端；viewers_ 按连上的先后排）：连着的人变了就告诉每个人画面听不听他的
void ShareHost::tellMainViewer()
{
    for (const Peer &viewer : std::as_const(viewers_))
        sendControl(viewer.info.id, rc::control::mainViewer(viewer.info.id == viewers_.first().info.id));
}

void ShareHost::receivedControl(int viewer, const QByteArray &message)
{
    if (const std::optional<bool> on = rc::control::parseLowLatency(message)) {
        const auto found = findId(viewers_, viewer);
        if (found == viewers_.end())
            return;
        found->lowLatency = *on;
        applyLowLatency();
    } else if (rc::control::isThumbnails(message)) {
        QList<quint32> ids;
        for (const rc::control::Display &display : displays_.list().displays)
            ids.append(display.id);
        if (session_ && !ids.isEmpty())
            thumbnails_.request(viewer, session_->handle(), ids);
    } else {
        displays_.received(viewer, message);
    }
}

void ShareHost::applyLowLatency()
{
    const bool wanted = !viewers_.isEmpty() && viewers_.first().lowLatency;
    if (!session_ || wanted == lowLatency_)
        return;
    if (session_->setPreset(wanted ? RCS_PRESET_LOW_LATENCY : RCS_PRESET_BALANCED) != RCS_OK) {
        qWarning("换低延迟模式：%s", rcsdk::lastCallError().c_str());
        return;
    }
    lowLatency_ = wanted;
    qInfo("%s低延迟模式", wanted ? "打开" : "关掉");
}

void ShareHost::sendControl(int viewer, const QByteArray &message)
{
    // 每个人各发一份：一个人的发送队列满了不耽误别人
    for (const Peer &peer : std::as_const(viewers_)) {
        if (viewer != 0 && peer.info.id != viewer)
            continue;
        if (sendData(peer.info.id, rc::peer::ControlChannel, message) == rc::peer::SendResult::QueueFull)
            qInfo("发给 %s 的控制消息没发出去：排着的太多了", qUtf8Printable(peer.info.name));
    }
}

void ShareHost::readDisplays()
{
    displays_.setDisplays(localDisplays(*session_));
}

bool ShareHost::showDisplay(quint32 id)
{
    auto capture = session_->settings<rcs_capture_config>();
    capture.display_id = id;
    if (session_->update(capture) == RCS_OK)
        return true;
    qWarning("换不到显示器 %u：%s", id, rcsdk::lastCallError().c_str());
    return false;
}

void ShareHost::showMainIfNobodyWatches()
{
    if (viewers_.isEmpty() && returning_.isEmpty())
        displays_.showMain();
}
