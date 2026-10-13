#include "rc/data_channels.h"

namespace rc::peer {

void DataChannels::add(std::uint32_t channel, Handler handler)
{
    Q_ASSERT(!handlers_.contains(channel));
    handlers_.insert(channel, std::move(handler));
}

void DataChannels::receive(std::uint32_t channel, int viewer, const QByteArray &data) const
{
    const auto handler = handlers_.constFind(channel);
    if (handler == handlers_.cend()) {
        qWarning("数据通道 %u 上收到 %lld 字节，没有人处理", channel, qlonglong(data.size()));
        return;
    }
    (*handler)(viewer, data);
}

} // namespace rc::peer
