#pragma once

#include "files/file_transfer_list.h"

#include <rc/file_transfers.h>

// 会话里互传的文件（ViewerSession 的 files）：会话窗口侧栏的"文件"页，对方发来时画面上的提示。
// 传输在本程序的观看会话上（rc::FileTransfers）；断开以后再连上还是这一份列表，断开时没传完的算失败，
// 重试时接着传。和这台设备上次（程序重开以前）没传完的也在列表里
class ViewerFiles : public FileTransferList
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("是 ViewerSession 的 files")

public:
    // interrupted：没传完的记在哪里（所有会话共用）；peer：对方（设备 ID，或者按 IP 连的地址；空的：不记）
    ViewerFiles(rc::InterruptedTransfers &interrupted, const QString &peer, QObject *parent);

    // 观看会话那边（ViewerSession）：会话建好了、要销毁了，SDK 的事件（见 rc::FileTransfers）
    void attach(rcs_session session) { files_.attach(session); }
    void detach() { files_.detach(); }
    void handle(const rc::FileEvent &event) { files_.handle(event); }

    // 拖进来的、选的文件和文件夹发给对方。返回空字符串表示发了，否则是给人看的原因。
    // 手机的文件选择器给的地址 SDK 读不了：在后台复制一份（mobile_files）再发，复制不了时 sendFailed
    Q_INVOKABLE QString send(const QList<QUrl> &urls);
    Q_INVOKABLE void clearCompleted();

    void reject(int id) override;
    void cancel(int id) override;
    void retry(int id) override;
    void remove(int id) override;

signals:
    void sendFailed(const QString &message);

protected:
    void acceptInto(int id, const QString &directory) override;

private:
    rc::FileTransfers files_;
};
