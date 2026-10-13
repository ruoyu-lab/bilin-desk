#pragma once

#include <QList>
#include <QString>

namespace rc {

// 对话里的一句话
struct ChatMessage
{
    bool incoming = false; // 对方说的
    QString text;
    qint64 time = 0; // 本机收到、发出的时间（毫秒）
    QString error;   // 自己说的没发出去时为什么；发出去了是空的

    friend bool operator==(const ChatMessage &, const ChatMessage &) = default;
};

// 本机和各台设备的对话记录（界面程序的），directory 下每台设备一个文件：不管谁连谁，和同一台设备的都在一起。
// peer 是对方，和设备列表的 key 一样：设备 ID（9 位数字）；按 IP 地址连的、不知道设备 ID 的是 "ip:地址"。
// 每台只留最近 Limit 条
class ChatHistory
{
public:
    static constexpr qsizetype Limit = 500;

    explicit ChatHistory(QString directory);

    QList<ChatMessage> load(const QString &peer) const;
    // 整个换掉，只存最后 Limit 条（先写到临时文件再替换）。存不了（磁盘满了、没有权限）时记一条警告、返回 false
    bool save(const QString &peer, const QList<ChatMessage> &messages) const;

private:
    QString path(const QString &peer) const;

    QString directory_;
};

} // namespace rc
