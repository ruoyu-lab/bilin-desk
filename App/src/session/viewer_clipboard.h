#pragma once

#include <rcsdk/clipboard.h>

#include <QCoreApplication>
#include <QString>

#include <optional>

namespace rcsdk {
class Session;
class SessionConfig;
}

// 剪贴板同步（去连别人的会话里）：SDK 的剪贴板模块读写本机的系统剪贴板，文字、HTML、RTF、图片两个方向都传。
// - 连上时本机已经复制着的也传过去：连之前复制的，连上就能在对方那里粘贴
// - 复制它的程序标了不要分享的（密码管理器）留在本机（SDK 默认）
// - 对方那一面也开着才同步（被控的人可以关掉；关了这里说"对方关了剪贴板同步"）
// 会话里随时开关，不重连（SDK 运行中改模块的设置）。同步不了时 note() 说一句为什么，没问题时是空的
class ViewerClipboard
{
    Q_DECLARE_TR_FUNCTIONS(ViewerClipboard)

public:
    // 剪贴板模块的一个事件里要的部分：在 SDK 的线程上复制出来（事件只在回调里有效），再交给界面线程
    struct Event
    {
        rcs_module_event_kind kind = 0;
        rcs_clipboard_flow flow = 0; // RCS_CLIPBOARD_COPY：本机复制的发出去，还是对方复制的放进本机
        rcs_result result = RCS_OK;
        QString message;
        bool peerTakes = true; // RCS_CLIPBOARD_PEER：对方收不收本机复制的（关了同步就不收）
    };
    // 不是剪贴板模块的事件时为空
    static std::optional<Event> read(const rcs_event &event);

    bool enabled() const { return enabled_; }
    QString note() const;

    // 新的一次连接用的设置；说明从头来
    void configure(rcsdk::SessionConfig &config);
    // 开关变了时返回 true。session：正在连、连着的会话，马上生效；没有时下一次连接用
    bool setEnabled(bool enabled, rcsdk::Session *session);
    // 说明变了时返回 true
    bool update(const Event &event);

private:
    rcs_clipboard_settings settings() const;

    bool enabled_ = true;
    bool peerOff_ = false; // 对方关了剪贴板同步
    QString note_;         // 上一次复制没同步过去的原因
};
