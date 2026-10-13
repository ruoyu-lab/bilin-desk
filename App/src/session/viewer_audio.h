#pragma once

#include <rcsdk/rcsdk.h>

#include <QCoreApplication>
#include <QString>

namespace rcsdk {
class Session;
class SessionConfig;
}

// 远程声音（去连别人的会话里）：对方电脑正在播放的声音，SDK 在本机默认的输出设备上播放（换了设备跟着换）。
// 会话里随时开关，不重连（SDK 运行中改收声音的设置）。开着却听不到时 note() 说一句为什么，没问题时是空的：
// 本机这一头的原因（SDK 的事件）优先，其次是对方录不了（对方发来的，rc/control_protocol.h 的 sound）
class ViewerAudio
{
    Q_DECLARE_TR_FUNCTIONS(ViewerAudio)

public:
    bool enabled() const { return enabled_; }
    QString note() const;

    // 新的一次连接用的设置；说明从头来
    void configure(rcsdk::SessionConfig &config);
    // 开关变了时返回 true。session：正在连、连着的会话，马上生效；没有时下一次连接用
    bool setEnabled(bool enabled, rcsdk::Session *session);
    // SDK 的声音事件（收的方向）；说明变了时返回 true
    bool update(const rcs_audio_event &event, const rcsdk::Session &session);
    // 对方录不了声音时为什么（空的：录得了）；说明变了时返回 true
    bool setPeerProblem(const QString &problem);

private:
    bool enabled_ = true;
    QString localNote_;
    QString peerProblem_;
};
