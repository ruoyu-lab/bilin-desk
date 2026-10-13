#pragma once

#include <rc/transfer_list.h>

#include <QList>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// 界面上的一份互传文件：会话里的（ViewerFiles，传输在本程序的观看会话上），或者别人连本机时的
// （IncomingFiles，传输在后台服务里）。两边一样的在这里：给 QML 的列表、给人看的说明，收下之前看看存到的地方，
// 在访达 / 资源管理器里显示收到的。
//   transfers：本端的传输（不含 offers），每一项 { id, viewer, sending, name（"a.txt 等 3 项"）, status, totalBytes,
//              doneBytes, bytesPerSecond, files, message（结束了时给人看的说明）, folder（收到的存在哪个文件夹，
//              给人看的名字）, savedPath, resumable（没传完、下次接着传）, resumed（接着上次的） }，
//              status 见 rc/transfer_list.h
//   offers：对方发来、等本机的人决定收不收的，每一项同上（files 是一共几个文件；resumed 的存回上次的文件夹）
// 传输中 transfers 每秒变几次（进度），offers 只在有人发来、决定了的时候变：分开通知，问收不收的卡片不跟着重建
class FileTransferList : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("是 ViewerSession、IncomingConnections 的 files")

    Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)
    Q_PROPERTY(QVariantList offers READ offers NOTIFY offersChanged)

public:
    QVariantList transfers() const { return transfers_; }
    QVariantList offers() const { return offers_; }

    // 收下 offers 里的 id，存到 folder（设置里的"收到的文件存到"，空的是系统的"下载"）；接着上次传的存回上次的
    // 文件夹（它不在了就存到 folder，从头收）。
    // 返回空字符串表示收下了，否则是给人看的原因（文件夹不在了、空间不够），这时还等着决定
    Q_INVOKABLE QString accept(int id, const QUrl &folder);
    Q_INVOKABLE virtual void reject(int id) = 0;
    // 取消没结束的
    Q_INVOKABLE virtual void cancel(int id) = 0;
    // 再发一次没发成的：没发完的接着发
    Q_INVOKABLE virtual void retry(int id) = 0;
    // 从列表里去掉结束了的：没收完的不要了，删掉收了一半的
    Q_INVOKABLE virtual void remove(int id) = 0;
    // 在访达 / 资源管理器里显示收到的
    Q_INVOKABLE void showInFolder(int id) const;

signals:
    void transfersChanged();
    void offersChanged();

protected:
    using QObject::QObject;

    // 新的列表
    void setTransfers(const QList<rc::Transfer> &transfers);
    virtual void acceptInto(int id, const QString &directory) = 0;
    // 拖进来的、文件对话框选的地址变成本机的绝对路径；有不是本机文件的时返回给人看的原因
    static QString localPaths(const QList<QUrl> &urls, QStringList &paths);

private:
    const rc::Transfer *find(int id) const;
    static QVariantMap toQml(const rc::Transfer &transfer);
    static QString message(const rc::Transfer &transfer);
    static QString folderName(const QString &directory);

    QList<rc::Transfer> list_;
    QVariantList transfers_;
    QVariantList offers_;
};
