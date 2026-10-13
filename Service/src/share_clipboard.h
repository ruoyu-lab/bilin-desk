#pragma once

#include <rcsdk/clipboard.h>

namespace rcsdk {
class Session;
class SessionConfig;
}

// 被控这一面的剪贴板同步：分享会话里的剪贴板模块读写本机的系统剪贴板，文字、HTML、RTF、图片两个方向都传，
// 连着的人共用。本机的人可以关掉（所有人一起）：本机复制的不发出去，对方复制的也不放到本机；
// 对方那一面看得出这边不收了（SDK 告诉它），说"对方关了剪贴板同步"。
// - 复制它的程序标了不要分享的（密码管理器）留在本机（SDK 默认）
// - 连上时本机已经复制着的不传：连上不改对方的剪贴板，之后复制的才传
// - 对方那一面也开着才同步
class ShareClipboard
{
public:
    bool enabled() const { return enabled_; }

    // 分享会话用剪贴板模块，按现在开没开
    void configure(rcsdk::SessionConfig &config) const;
    // 开关变了时返回 true。session：正在运行的分享会话，马上生效；没有时下一次开分享会话时用
    bool setEnabled(bool enabled, rcsdk::Session *session);

private:
    rcs_clipboard_settings settings() const;

    bool enabled_ = true;
};
