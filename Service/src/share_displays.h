#pragma once

#include <rc/control_protocol.h>

#include <QByteArray>
#include <QList>

#include <functional>

// 分享端的多显示器：一次分享一块，默认主显示器。观看端拿不到本机的显示器（SDK 只在分享端列得出），
// 由这里经数据通道告诉它们（rc/control_protocol.h），观看端要看哪一块就换过去。
// 分享会话是所有观看端共用的：能控制本机的人才能换（只能看的人换了会打断正在控制的人），换了大家都跟着换，
// 换完告诉所有人。
// 不碰 SDK：显示器由 ShareHost 给，换显示器、发消息经构造时给的函数交回 ShareHost
class ShareDisplays
{
public:
    // show：改成分享这块显示器（0 是主显示器），成功时 true；send：发给观看端 viewer（0：所有连着的）；
    // canChoose：观看端 viewer 能不能换（能控制本机）
    ShareDisplays(std::function<bool(quint32 id)> show, std::function<void(int viewer, const QByteArray &message)> send,
                  std::function<bool(int viewer)> canChoose);

    // 分享会话重新开了：分享主显示器，还不知道有哪几块
    void reset();
    // 本机现在的显示器（观看端连进来时、显示器变了时）。正在分享的那块没了就换回主显示器；有变化就告诉所有人
    void setDisplays(const QList<rc::control::Display> &displays);
    // 观看端 viewer 连进来了：告诉它有哪几块
    void viewerJoined(int viewer);
    // 观看端 viewer 经数据通道发来的控制消息
    void received(int viewer, const QByteArray &message);
    // 没人在看了：下一个连进来的人从主显示器看起
    void showMain();

    rc::control::DisplayList list() const;

private:
    bool has(quint32 id) const;

    std::function<bool(quint32)> show_;
    std::function<void(int, const QByteArray &)> send_;
    std::function<bool(int)> canChoose_;
    QList<rc::control::Display> displays_;
    quint32 chosen_ = 0; // 观看端选的那块；0：主显示器
};
