#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// 没传完、留着下次接着传的互传文件（连接断了、程序退出了、电脑关了），两个方向都有，存在一个 JSON 文件里：
//   {"transfers": [{"token": 十六进制, "peer", "sending", "name", "entries", "files", "totalBytes", "doneBytes",
//                   "paths": [...]（发）, "directory"（收）, "at": ISO 时间, "discard"（收）}...]}
// 开始传时记下，传完了、取消了、被拒绝了时去掉，所以程序中途没了也留着。同一次传输认 SDK 给的 token
// （rcs_file_transfer_info.token）：发的一端用它把同样的东西再发一次，收的一端认出是接着上次的，存回上次的文件夹，
// 半截文件在那里接着写（SDK 的文件传输模块，keep_partial）。
// 收的不要了（discard）：等 SDK 的会话在运行时删掉留着的半截文件（rcs_file_discard），删了再去掉。
// 放了 KeepDays 天的不再等：发的直接去掉，收的当作不要了
namespace rc {

struct InterruptedTransfer
{
    QByteArray token; // SDK 的 RCS_FILE_TOKEN_SIZE 字节
    QString peer; // 对端：设备 ID，按 IP 连的是地址
    bool sending = false;
    QString name; // 第一项的名字
    int entries = 0;
    int files = 0;
    qint64 totalBytes = 0;
    qint64 doneBytes = 0;
    QStringList paths; // 发：本机的绝对路径
    QString directory; // 收：存到的文件夹
    QDateTime at; // 记下（开始传、断了）的时候
    bool discard = false; // 收：不要了，等着删掉半截文件

    friend bool operator==(const InterruptedTransfer &, const InterruptedTransfer &) = default;
};

class InterruptedTransfers
{
public:
    static constexpr int KeepDays = 30;

    // file 不存在时是空的；now 用来去掉放太久的
    explicit InterruptedTransfers(QString file, const QDateTime &now = QDateTime::currentDateTimeUtc());

    // 和 peer 之间没传完的（不算不要了的）
    QList<InterruptedTransfer> forPeer(const QString &peer) const;
    const InterruptedTransfer *find(const QByteArray &token) const;
    // 记下（同一个 token 的换掉）、去掉、不要了（收的等着删掉半截文件，发的直接去掉）
    void put(const InterruptedTransfer &transfer);
    void remove(const QByteArray &token);
    void discard(const QByteArray &token);
    // 收的、要删掉半截文件的
    QList<InterruptedTransfer> discarded() const;
    // 发的、还要接着发的那些本机路径（手机上为发送复制出来的文件留着它们）
    QStringList sendingPaths() const;

private:
    void save() const;

    QString file_;
    QList<InterruptedTransfer> transfers_;
};

} // namespace rc
