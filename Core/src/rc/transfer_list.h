#pragma once

#include "rc/interrupted_transfers.h"

#include <QByteArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// 互传文件，两个程序共用、和 SDK 无关的部分：一次传输在界面上的样子，一个会话里的传输列表。
// SDK 那一层在 rc/file_transfers.h；分享端的列表经本机连接的 files 主题交给界面（rc/protocol.h）
namespace rc {

// Transfer 的 status 和 reason
namespace transfer {

// ---- status ----

// 发：在列出要发的文件；收：接收了，在查存到的地方
inline constexpr QLatin1StringView Preparing{"preparing"};
// 发：等对方接收；收：等本机的人决定收不收
inline constexpr QLatin1StringView Offered{"offered"};
inline constexpr QLatin1StringView Running{"running"};
inline constexpr QLatin1StringView Paused{"paused"};
// 连接断了，等对方回来（SDK 等一分钟，回来接着传）
inline constexpr QLatin1StringView Waiting{"waiting"};
// 结束了（reason 说为什么没完成）
inline constexpr QLatin1StringView Completed{"completed"};
inline constexpr QLatin1StringView Rejected{"rejected"};
inline constexpr QLatin1StringView Cancelled{"cancelled"};
inline constexpr QLatin1StringView Failed{"failed"};

// ---- reason：结束了却没传完时为什么；空的是没什么要说的（完成了，自己取消的）或者说不清的错 ----

// rejected：收的人点了拒绝
inline constexpr QLatin1StringView Refused{"refused"};
// rejected：收的人一直没决定（SDK 的 offer 超时，两分钟）
inline constexpr QLatin1StringView TimedOut{"timeout"};
// rejected：收的一端没人能决定（被控电脑上没开界面程序）
inline constexpr QLatin1StringView Unattended{"unattended"};
// rejected：对方还有太多没决定的
inline constexpr QLatin1StringView Busy{"busy"};
// cancelled：对方取消的
inline constexpr QLatin1StringView ByPeer{"byPeer"};
// failed：连接断了，对方没回来
inline constexpr QLatin1StringView Disconnected{"disconnected"};
// failed：对方的程序没有互传文件（版本不同）
inline constexpr QLatin1StringView Unsupported{"unsupported"};
// failed：本端读写文件出错（磁盘满了、文件夹不能写入）
inline constexpr QLatin1StringView Device{"device"};
// failed：对方读写文件出错
inline constexpr QLatin1StringView PeerDevice{"peerDevice"};
// failed：发不了：有的文件不在了、名字对方的系统存不了，或者一次超过 SDK 的上限（一万项）
inline constexpr QLatin1StringView NotSendable{"notSendable"};
// failed：对方的程序不守约定
inline constexpr QLatin1StringView Broken{"broken"};
// completed：有的没传成（missed 个）；failed：一个都没传成
inline constexpr QLatin1StringView Partial{"partial"};

} // namespace transfer

// 一次传输（一组文件和文件夹，一个方向，对一个对端）
struct Transfer
{
    int id = 0; // 本端的编号，一个会话里不重复
    int viewer = 0; // 分享端：和连着本机的哪个人（SharePeer 的 id，对方重连回来时跟着变）；观看端是 0
    bool sending = false;
    QString name; // 第一项（文件或文件夹）的名字
    int entries = 0; // 几项（不算文件夹里面的）
    int files = 0; // 收：一共几个文件（算上文件夹里面的）；发是 0
    qint64 totalBytes = 0;
    qint64 doneBytes = 0;
    qint64 bytesPerSecond = 0;
    QString status; // 见 transfer::
    QString reason;
    int missed = 0; // 结束了：没传成的文件（失败的，和符号链接这类跳过的）
    QString directory; // 收：存到哪个文件夹（接收以后；接着上次传的是上次的文件夹）
    QString savedPath; // 收、完成了：第一项存成了哪里
    // 结束了、没传完，留着下次接着传（rc/interrupted_transfers.h）：发的重试时接着传，收的等对方再发
    bool resumable = false;
    // 收：对方发来的是上次没传完的，接着传（存回上次的文件夹）
    bool resumed = false;

    bool finished() const;

    QJsonObject toJson() const;
    static Transfer fromJson(const QJsonObject &json);

    friend bool operator==(const Transfer &, const Transfer &) = default;
};

// files 主题的内容：{"transfers": [...]}
QJsonObject transfersToJson(const QList<Transfer> &transfers);
QList<Transfer> transfersFromJson(const QJsonObject &json);

// 一个会话里互传的文件（rc::FileTransfers 用）：本端的编号（界面、本机连接上用它）↔ SDK 的传输编号和
// 跨运行认出同一次传输的 token，发的那些本机路径（重试时再发一遍）。按 SDK 编号的更新，不认识的编号不管
// （比如本端拒绝了、已经去掉了的）。结束了的最多留 KeptFinished 个，多了去掉最早的
class TransferList
{
public:
    static constexpr int KeptFinished = 50;

    // 对方发来的一项：相对路径（"/" 分隔，文件夹里的在文件夹后面），是不是文件（不是就是文件夹，或者不传的符号链接）
    struct Item
    {
        QString path;
        bool file = false;
    };

    QList<Transfer> all() const;
    const Transfer *find(int id) const;
    const Transfer *findSdk(quint64 sdk) const;
    // id 在 SDK 里的编号；没结束的才有，否则是 0
    quint64 sdkId(int id) const;
    // 发：本机的绝对路径
    QStringList paths(int id) const;
    // 对方发来、等本机的人决定的
    QList<int> offers() const;
    // SDK 给这次传输的 token；还没有时是空的
    QByteArray token(int id) const;
    const Transfer *findToken(const QByteArray &token) const;

    // 发 paths 给 viewer（preparing），返回本端的编号；随后 started 或者 failed
    int addSending(int viewer, const QStringList &paths);
    // 上次没传完的（结束了、没传完），对 viewer
    int addInterrupted(int viewer, const InterruptedTransfer &interrupted);
    void started(int id, quint64 sdk, const QByteArray &token);
    void failed(int id, QLatin1StringView reason);
    // 再发一次没发成的（拒绝、取消、失败了的）：回到 preparing，随后 started 或者 failed。别的返回 false。
    // token 不变：没传完的接着传
    bool restart(int id);
    // 对方发来的（offered），返回本端的编号
    int offered(quint64 sdk, int viewer, const QList<Item> &items, qint64 totalBytes, const QByteArray &token);
    // 对方发来的是上次没传完的：存回上次的文件夹 directory，上次已经收了 doneBytes
    void resuming(int id, const QString &directory, qint64 doneBytes);
    // 本机的人接收了，存到 directory
    void accepted(int id, const QString &directory);

    // SDK 说的：状态变了（没结束）、进度、结束了（status、reason 见上面）
    void setStatus(quint64 sdk, int viewer, QLatin1StringView status);
    void setProgress(quint64 sdk, int viewer, qint64 doneBytes, qint64 totalBytes, qint64 bytesPerSecond);
    // 返回结束了的这一个（之后可能因为结束了的太多被去掉）；不认识的编号是 nullopt
    std::optional<Transfer> finish(quint64 sdk, int viewer, QLatin1StringView status, QLatin1StringView reason,
                                   int missed, qint64 doneBytes, const QString &savedPath, bool resumable);
    // 对端回来了、编号变了（分享端：连着本机的人重连）
    void setViewer(int id, int viewer);

    // 会话没了：没结束的都算失败（连接断了），已经开始的留着下次接着传；等本机决定的不要了；SDK 的编号都作废
    void detached();
    void remove(int id);
    void clearCompleted();
    void clear();

private:
    struct Entry
    {
        Transfer transfer;
        quint64 sdk = 0;
        QByteArray token;
        QStringList paths;
    };

    Entry *entry(int id);
    Entry *entrySdk(quint64 sdk);
    void end(Entry &entry);
    void prune();

    QList<Entry> entries_;
    int nextId_ = 1;
};

} // namespace rc
