#pragma once

#include "rc/interrupted_transfers.h"
#include "rc/transfer_list.h"

#include <rcsdk/file_transfer.h>

#include <QHash>
#include <QObject>
#include <QSet>

#include <optional>
#include <variant>

namespace rcsdk {
class SessionConfig;
}

namespace rc {

// SDK 的事件里和互传文件有关的（事件只在回调里有效：复制下来，交给对象的线程）
struct FileEvent
{
    // 模块和这一端能不能用（RCS_MODULE_ACTIVE / INACTIVE：对方的程序没有这个模块、版本不同）
    struct Peer
    {
        bool active = false;
        QString why;
    };

    int viewer = 0; // 分享端：哪个连着本机的人；观看端是 0
    std::variant<Peer, rcs_file_offer_event, rcs_file_state_event, rcs_file_progress_event, rcs_file_done_event,
                 rcs_file_discard_event>
        what;
};

// 一个 SDK 会话里互传的文件（SDK 的文件传输模块），观看端（App 的 ViewerSession）和分享端（Service 的 ShareHost）
// 共用。每次传输只对一个对端：分享端按连着本机的人（viewer），观看端对分享端是 0。
// - 两个方向都能发，文件和文件夹；文件由模块在本进程里读写（后台服务以当前用户运行，就是用户自己的权限）
// - 对方发来的每次都等本机的人决定（accept / reject），两分钟没决定 SDK 就当拒绝；没人能决定时
//   （setAttended(false)：被控电脑上没开界面程序）直接拒绝。重名的自动改名（"名字 (1).扩展名"）
// - 收到的文件标上"来自网络"（rc/download_mark.h），打开收到的程序时系统会提醒
// - 连接断了：SDK 等对方一分钟，回来从断的地方接着传；没回来、会话停了（程序退出了、电脑关了）算失败，
//   已经开始传的记下来（rc/interrupted_transfers.h，按对端设备）：下次和同一台设备连上时还在列表里，
//   发的重试时接着传（同一个 token），收的一端留着半截文件，对方接着发来时存回上次的文件夹接着写。
//   收的不要了（从列表里去掉）就删掉半截文件
// 线程：read() 在 SDK 的线程上（会话的 on_event 里），宿主把它的结果搬到这个对象的线程交给 handle()，
// 只交这个会话的（宿主自己认得出哪些事件是已经停了的会话的）。其余都在这个对象的线程上
class FileTransfers : public QObject
{
    Q_OBJECT

public:
    // interrupted：没传完的记在哪里（一个程序一份，几个会话共用）
    explicit FileTransfers(InterruptedTransfers &interrupted, QObject *parent = nullptr);

    // 会话的配置里用上这个模块（进程里要先注册：rcs_file_transfer_module）
    static void configure(rcsdk::SessionConfig &config);
    // SDK 的线程上：是互传文件的事件就复制下来
    static std::optional<FileEvent> read(const rcs_event &event);

    // 会话建好了（start 之前）；要销毁了（没结束的传输都算失败）
    void attach(rcs_session session);
    void detach();
    void handle(const FileEvent &event);
    // 有没有人能决定收不收：没有时对方发来的直接拒绝，等着决定的也拒绝
    void setAttended(bool attended);
    // viewer 是哪台设备（设备 ID；按 IP 连的是地址）：连上的时候说。和它上次没传完的放进列表，
    // 列表里已经有的跟着它（分享端：重连回来的人编号变了）。没说的对端，没传完的不记
    void setPeer(int viewer, const QString &peer);

    QList<Transfer> transfers() const { return list_.all(); }

    // 把本机的这些文件、文件夹（绝对路径）发给 viewer，返回本端的编号；发不出去的也在列表里，带着原因
    int send(int viewer, const QStringList &paths);
    // 对方发来的（offered）收下，存到 directory（本机已有的文件夹，绝对路径；接着上次传的是上次的文件夹）
    void accept(int id, const QString &directory);
    void reject(int id);
    // 取消没结束的
    void cancel(int id);
    // 再发一次没发成的：没传完的接着传
    void retry(int id);
    // 从列表里去掉结束了的；收的没传完的不要了，删掉半截文件
    void remove(int id);
    void clearCompleted();
    void clear();

signals:
    void changed();
    // 传完了一份（发的或者收的）
    void completed(const rc::Transfer &transfer);

private:
    void start(int id);
    void offered(int viewer, const rcs_file_offer_event &offer);
    void finished(int viewer, const rcs_file_done_event &done);
    QList<TransferList::Item> items(quint64 transfer) const;
    QString savedPath(quint64 transfer) const;
    void markReceived(quint64 transfer) const;
    // 记下没传完的（开始传了、接收了、断了）：列表里的 id，或者结束了的 transfer（可能已经不在列表里）；
    // 传完了、不再接着传的去掉
    void remember(int id);
    void remember(const Transfer &transfer, const QByteArray &token, const QStringList &paths);
    void forget(const QByteArray &token);
    // 不要了的半截文件：会话运行着、模块在本机跑着时删掉
    void discardLeftovers();

    InterruptedTransfers &interrupted_;
    rcs_session session_ = nullptr;
    bool attended_ = true;
    // 分享端的每个人 / 观看端的对方（0）是哪台设备
    QHash<int, QString> peers_;
    // 对方的程序不能互传文件的（RCS_MODULE_INACTIVE）
    QSet<int> unsupported_;
    TransferList list_;
};

} // namespace rc
