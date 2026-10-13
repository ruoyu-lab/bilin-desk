#include "viewer_clipboard.h"

#include <rcsdk/rcsdk.hpp>

#include <cstring>

std::optional<ViewerClipboard::Event> ViewerClipboard::read(const rcs_event &event)
{
    if (event.type != RCS_EVENT_MODULE || std::strcmp(event.module.module, RCS_CLIPBOARD_MODULE) != 0)
        return std::nullopt;
    const rcs_module_event &module = event.module;
    if (module.kind == RCS_CLIPBOARD_PEER) {
        const auto &peer = *static_cast<const rcs_clipboard_peer *>(module.payload);
        return Event{module.kind, 0, module.result, {}, peer.formats != 0};
    }
    if (module.kind != RCS_CLIPBOARD_COPY)
        return Event{module.kind, 0, module.result, QString::fromUtf8(module.message)};
    const auto &copy = *static_cast<const rcs_clipboard_event *>(module.payload);
    return Event{module.kind, copy.flow, copy.result, QString::fromUtf8(copy.message)};
}

void ViewerClipboard::configure(rcsdk::SessionConfig &config)
{
    config.useModule(RCS_CLIPBOARD_MODULE, settings());
    peerOff_ = false;
    note_.clear();
}

QString ViewerClipboard::note() const
{
    return peerOff_ ? tr("对方关了剪贴板同步") : note_;
}

bool ViewerClipboard::setEnabled(bool enabled, rcsdk::Session *session)
{
    if (enabled == enabled_)
        return false;
    enabled_ = enabled;
    note_.clear();
    if (session) {
        rcsdk::SessionConfig change(RCS_ROLE_VIEW);
        change.useModule(RCS_CLIPBOARD_MODULE, settings());
        if (session->update(change) != RCS_OK)
            qWarning("剪贴板同步改不了：%s", rcsdk::lastCallError().c_str());
    }
    return true;
}

bool ViewerClipboard::update(const Event &event)
{
    if (event.kind == RCS_CLIPBOARD_PEER) {
        if (peerOff_ == !event.peerTakes)
            return false;
        peerOff_ = !event.peerTakes;
        qInfo("对方%s剪贴板同步", peerOff_ ? "关了" : "开了");
        return true;
    }
    QString note;
    switch (event.kind) {
    case RCS_MODULE_ACTIVE:
        break;
    case RCS_MODULE_INACTIVE:
        note = tr("对方的程序版本不一样，同步不了剪贴板");
        break;
    case RCS_CLIPBOARD_COPY:
        if (event.result == RCS_ERROR_BUFFER_TOO_SMALL)
            note = tr("复制的内容太大，没有同步过去");
        else if (event.result == RCS_ERROR_DEVICE && event.flow == RCS_CLIPBOARD_RECEIVE)
            note = tr("对方复制的内容放不进本机的剪贴板");
        else if (event.result == RCS_ERROR_DEVICE)
            note = tr("读不了本机的剪贴板，复制的没有同步过去");
        else if (event.result != RCS_OK)
            note = tr("出错了，复制的没有同步过去");
        break;
    default:
        return false;
    }
    if (note == note_)
        return false;
    if (!note.isEmpty())
        qInfo("剪贴板同步：%s（%s）", qUtf8Printable(note), qUtf8Printable(event.message));
    note_ = note;
    return true;
}

// 关着时什么都不传；连上时本机已经复制着的也传过去（send_existing）
rcs_clipboard_settings ViewerClipboard::settings() const
{
    rcs_clipboard_settings settings = rcs_clipboard_settings_init();
    settings.flow = enabled_ ? RCS_CLIPBOARD_SEND | RCS_CLIPBOARD_RECEIVE : 0u;
    settings.send_existing = 1;
    return settings;
}
