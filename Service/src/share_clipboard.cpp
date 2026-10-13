#include "share_clipboard.h"

#include <rcsdk/rcsdk.hpp>

#include <QtGlobal>

void ShareClipboard::configure(rcsdk::SessionConfig &config) const
{
    config.useModule(RCS_CLIPBOARD_MODULE, settings());
}

bool ShareClipboard::setEnabled(bool enabled, rcsdk::Session *session)
{
    if (enabled == enabled_)
        return false;
    enabled_ = enabled;
    if (session) {
        rcsdk::SessionConfig change(RCS_ROLE_SHARE);
        change.useModule(RCS_CLIPBOARD_MODULE, settings());
        if (session->update(change) != RCS_OK) {
            qWarning("剪贴板同步改不了：%s", rcsdk::lastCallError().c_str());
            enabled_ = !enabled;
            return false;
        }
    }
    return true;
}

// 关着时什么都不传
rcs_clipboard_settings ShareClipboard::settings() const
{
    rcs_clipboard_settings settings = rcs_clipboard_settings_init();
    settings.flow = enabled_ ? RCS_CLIPBOARD_SEND | RCS_CLIPBOARD_RECEIVE : 0u;
    return settings;
}
