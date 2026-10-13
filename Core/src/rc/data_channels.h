#pragma once

#include <QByteArray>
#include <QHash>

#include <cstdint>
#include <functional>

namespace rc::peer {

// 往数据通道上发一条的结果（SDK 的 rcs_session_send_data 归成这几种，不用 SDK 的地方也能处理）
enum class SendResult {
    Sent,
    // 排着要发给对方的太多了（SDK 的发送队列满了），过一会儿再发
    QueueFull,
    // 没连着：还没连上、断开了，或者正在重连
    NotConnected,
};

// 数据通道上收到的消息按通道号（rc/peer_protocol.h）交给各自的处理者。在会话对象的线程上用：
// SDK 读连接的线程上收到的先复制、搬过来再交给它
class DataChannels
{
public:
    // viewer：分享端是发来的那个观看端（SDK 的 viewer_id），观看端总是 0
    using Handler = std::function<void(int viewer, const QByteArray &data)>;

    void add(std::uint32_t channel, Handler handler);
    // 没人处理的通道记一条警告后丢掉：两端是同一个版本的程序，只有对方出了错才会有
    void receive(std::uint32_t channel, int viewer, const QByteArray &data) const;

private:
    QHash<std::uint32_t, Handler> handlers_;
};

} // namespace rc::peer
