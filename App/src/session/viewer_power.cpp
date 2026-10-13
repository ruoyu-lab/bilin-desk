#include "viewer_power.h"

#include <rc/power_actions.h>

#include <rcsdk/rcsdk.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

// 没做成的在画面上留多久
constexpr int ShowMs = 5000;

} // namespace

std::optional<ViewerPower::Event> ViewerPower::read(const rcs_event &event)
{
    if (event.type != RCS_EVENT_MODULE || std::strcmp(event.module.module, RCS_POWER_MODULE) != 0)
        return std::nullopt;
    const rcs_module_event &module = event.module;
    Event copy;
    copy.kind = module.kind;
    if (module.kind == RCS_POWER_PEER) {
        const auto *peer = static_cast<const rcs_power_peer *>(module.payload);
        copy.peerActions = peer->actions;
        copy.wakeInfo = peer->wake_info != 0;
    }
    else if (module.kind == RCS_POWER_ACTION)
        copy.action = *static_cast<const rcs_power_action_event *>(module.payload);
    return copy;
}

ViewerPower::ViewerPower(QObject *parent)
    : QObject(parent)
{
    clearTimer_.setSingleShot(true);
    clearTimer_.setInterval(ShowMs);
    connect(&clearTimer_, &QTimer::timeout, this, [this] { setPhase({}); });
}

QVariantMap ViewerPower::request() const
{
    if (phase_.isEmpty())
        return {};
    return {
        {QStringLiteral("action"), action_},
        {QStringLiteral("phase"), phase_},
        {QStringLiteral("message"), message_},
    };
}

void ViewerPower::configure(rcsdk::SessionConfig &config)
{
    rcs_power_settings settings = rcs_power_settings_init();
    settings.keep_awake = RCS_KEEP_AWAKE_OFF;
    config.useModule(RCS_POWER_MODULE, settings);
    reset();
}

void ViewerPower::ask(const QString &action, rcsdk::Session &session)
{
    if (phase_ == u"asking" || phase_ == u"accepted" || !offered_.contains(action))
        return;
    action_ = action;
    send(session, false);
}

void ViewerPower::send(rcsdk::Session &session, bool force)
{
    forced_ = force;
    const rcs_result result = rcs_power_request_action(session.handle(), *rc::power::action(action_),
                                                       force ? RCS_POWER_FORCE : 0u, nullptr);
    if (result == RCS_OK)
        setPhase(QStringLiteral("asking"));
    else
        setPhase(QStringLiteral("failed"), tr("没能交给对方电脑：%1").arg(QString::fromUtf8(rcs_result_message(result))));
}

void ViewerPower::handle(const Event &event, rcsdk::Session &session)
{
    if (event.kind == RCS_POWER_PEER) {
        offered_ = rc::power::names(event.peerActions);
        emit changed();
        if (event.wakeInfo) {
            std::vector<rcs_power_interface> items(RCS_POWER_INTERFACES_MAX);
            uint32_t count = 0;
            if (rcs_power_get_peer_interfaces(session.handle(), items.data(), sizeof(rcs_power_interface),
                                              uint32_t(items.size()), &count)
                == RCS_OK)
                emit wakeInterfaces(QList<rcs_power_interface>(items.begin(), items.begin() + std::min<size_t>(count, items.size())));
        }
        return;
    }
    if (event.kind != RCS_POWER_ACTION || phase_ != u"asking")
        return;
    const rcs_power_action_event &action = event.action;
    switch (action.state) {
    case RCS_POWER_ACTION_ACCEPTED:
    case RCS_POWER_ACTION_STARTED:
        leaving_ = action_;
        setPhase(QStringLiteral("accepted"));
        break;
    case RCS_POWER_ACTION_REFUSED:
    case RCS_POWER_ACTION_FAILED:
        // 对方做不了不强制的：再强制一次（那里没有会问保不保存的程序，或者只能这样做）
        if (action.state == RCS_POWER_ACTION_REFUSED && action.result == RCS_ERROR_UNSUPPORTED && !forced_) {
            qInfo("对方不能不强制%s（%s），强制再要一次", qUtf8Printable(action_), action.message);
            send(session, true);
            break;
        }
        qWarning("让对方%s没成：%s", qUtf8Printable(action_), action.message);
        setPhase(QStringLiteral("failed"),
                 action.result == RCS_ERROR_UNSUPPORTED ? tr("对方电脑的系统做不到")
                 : action.result == RCS_ERROR_INVALID_STATE ? tr("对方电脑现在做不了：没有人登录，或者还没准备好")
                 : action.result == RCS_ERROR_BUSY ? tr("对方电脑正在做另一个")
                 : action.result == RCS_ERROR_REFUSED ? tr("现在没有控制对方电脑")
                 : tr("对方电脑出错了"));
        break;
    default:
        break;
    }
}

void ViewerPower::reset()
{
    clearTimer_.stop();
    const bool had = !offered_.isEmpty() || !phase_.isEmpty() || !leaving_.isEmpty();
    offered_.clear();
    action_.clear();
    forced_ = false;
    phase_.clear();
    message_.clear();
    leaving_.clear();
    if (had)
        emit changed();
}

void ViewerPower::setPhase(const QString &phase, const QString &message)
{
    phase_ = phase;
    message_ = message;
    if (phase.isEmpty())
        action_.clear();
    if (phase == u"failed")
        clearTimer_.start();
    else
        clearTimer_.stop();
    emit changed();
}
