#include "share_displays.h"

#include <algorithm>

namespace control = rc::control;

ShareDisplays::ShareDisplays(std::function<bool(quint32)> show, std::function<void(int, const QByteArray &)> send,
                             std::function<bool(int)> canChoose)
    : show_(std::move(show))
    , send_(std::move(send))
    , canChoose_(std::move(canChoose))
{
}

void ShareDisplays::reset()
{
    displays_.clear();
    chosen_ = 0;
}

void ShareDisplays::setDisplays(const QList<control::Display> &displays)
{
    if (displays == displays_)
        return;
    displays_ = displays;
    // 正在分享的那块拔掉了：不换的话 SDK 过十秒就停下整个分享会话
    if (chosen_ != 0 && !has(chosen_)) {
        qInfo("正在分享的显示器 %u 不在了，换回主显示器", chosen_);
        chosen_ = 0;
        show_(0);
    }
    send_(0, control::displays(list()));
}

void ShareDisplays::viewerJoined(int viewer)
{
    send_(viewer, control::displays(list()));
}

void ShareDisplays::received(int viewer, const QByteArray &message)
{
    const std::optional<quint32> id = control::parseShowDisplay(message);
    if (!id) {
        qWarning("观看端 %d 发来不认识的控制消息", viewer);
        return;
    }
    if (*id == list().current)
        return;
    // 它那边按钮是灰的；正好在它被改成只能看的时候点的
    if (!canChoose_(viewer)) {
        qInfo("观看端 %d 只能看，不能换显示器", viewer);
        return;
    }
    // 刚拔掉的：显示器变了的消息已经发给它了
    if (!has(*id)) {
        qInfo("观看端 %d 要看的显示器 %u 已经不在了", viewer, *id);
        return;
    }
    if (!show_(*id))
        return;
    qInfo("观看端 %d 换成看显示器 %u", viewer, *id);
    chosen_ = *id;
    send_(0, control::displays(list()));
}

void ShareDisplays::showMain()
{
    if (chosen_ == 0)
        return;
    chosen_ = 0;
    show_(0);
}

control::DisplayList ShareDisplays::list() const
{
    control::DisplayList list{displays_, chosen_};
    if (chosen_ == 0) {
        const auto main = std::find_if(displays_.cbegin(), displays_.cend(), [](const control::Display &d) { return d.main; });
        list.current = main == displays_.cend() ? 0 : main->id;
    }
    return list;
}

bool ShareDisplays::has(quint32 id) const
{
    return std::any_of(displays_.cbegin(), displays_.cend(), [id](const control::Display &d) { return d.id == id; });
}
