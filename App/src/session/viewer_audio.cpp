#include "viewer_audio.h"

#include <rc/control_protocol.h>

#include <rcsdk/rcsdk.hpp>

QString ViewerAudio::note() const
{
    if (!enabled_)
        return {};
    if (!localNote_.isEmpty())
        return localNote_;
    if (peerProblem_.isEmpty())
        return {};
    if (peerProblem_ == rc::control::SoundNoDevice)
        return tr("对方电脑没有能用的声音设备，传不了声音");
    if (peerProblem_ == rc::control::SoundNoPermission)
        return tr("对方电脑没有录声音的授权");
    return tr("对方电脑录不了声音");
}

void ViewerAudio::configure(rcsdk::SessionConfig &config)
{
    config.setReceiveAudio(enabled_);
    localNote_.clear();
    peerProblem_.clear();
}

bool ViewerAudio::setEnabled(bool enabled, rcsdk::Session *session)
{
    if (enabled == enabled_)
        return false;
    enabled_ = enabled;
    localNote_.clear();
    if (session) {
        rcs_receive_audio_config receive = rcsdk::defaults<rcs_receive_audio_config>();
        receive.enabled = enabled ? 1u : 0u;
        if (session->update(receive) != RCS_OK)
            qWarning("远程声音改不了：%s", rcsdk::lastCallError().c_str());
    }
    return true;
}

bool ViewerAudio::update(const rcs_audio_event &event, const rcsdk::Session &session)
{
    QString local;
    if (enabled_ && !event.active) {
        if (event.result == RCS_OK) // 有一端关着：本机开着，那就是对方不传
            local = session.peerInfo().sends_audio ? QString() : tr("对方电脑不传声音过来");
        else if (event.result == RCS_ERROR_DEVICE)
            local = tr("本机的扬声器或耳机用不了，听不到对方的声音");
        else
            local = tr("出错了，本机放不了对方的声音");
    }
    if (local == localNote_)
        return false;
    if (!local.isEmpty())
        qInfo("远程声音：%s（%s）", qUtf8Printable(local), event.message);
    const QString before = note();
    localNote_ = local;
    return note() != before;
}

bool ViewerAudio::setPeerProblem(const QString &problem)
{
    if (problem == peerProblem_)
        return false;
    const QString before = note();
    peerProblem_ = problem;
    if (!problem.isEmpty())
        qInfo("对方录不了声音（%s）", qUtf8Printable(problem));
    return note() != before;
}
