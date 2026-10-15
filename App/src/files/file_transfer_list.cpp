#include "file_transfer_list.h"

#ifdef Q_OS_ANDROID
#include "mobile/android_files.h"

#include <QDesktopServices>
#elif defined(Q_OS_IOS)
#include "mobile/ios_files.h"
#endif

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>

namespace transfer = rc::transfer;

namespace {

QString downloads()
{
#ifdef Q_OS_ANDROID
    return android_files::downloads();
#elif defined(Q_OS_IOS)
    return ios_files::downloads();
#else
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#endif
}

QString bytes(qint64 count)
{
    return QLocale().formattedDataSize(count, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace

QString FileTransferList::accept(int id, const QUrl &folder)
{
    const rc::Transfer *offer = find(id);
    // 刚好过期了、对方取消了：列表随后就变
    if (!offer)
        return {};
    const QString chosen = folder.isEmpty() ? downloads() : folder.toLocalFile();
    const QString directory = offer->resumed && QFileInfo(offer->directory).isDir() ? offer->directory : chosen;
    const QFileInfo info(directory);
    if (!info.isDir())
        return tr("存到的文件夹“%1”不在了，到设置里换一个").arg(info.fileName());
    if (!info.isWritable())
        return tr("存到的文件夹“%1”不能写入，到设置里换一个").arg(folderName(directory));
    const qint64 needed = offer->totalBytes - (directory == offer->directory ? offer->doneBytes : 0);
    const qint64 available = QStorageInfo(directory).bytesAvailable();
    if (available < needed)
        return tr("存不下：要 %1，“%2”所在的磁盘只剩 %3").arg(bytes(needed), folderName(directory), bytes(available));
    acceptInto(id, directory);
    return {};
}

void FileTransferList::showInFolder(int id) const
{
    const rc::Transfer *transfer = find(id);
    if (!transfer || transfer->savedPath.isEmpty())
        return;
    const QString path = QDir::toNativeSeparators(transfer->savedPath);
#ifdef Q_OS_MACOS
    QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-R"), path});
#elif defined(Q_OS_ANDROID)
    // 手机上没有"在文件夹中显示"：用能打开它的程序打开
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#elif defined(Q_OS_IOS)
    ios_files::open(path);
#else
    // "/select," 和路径分成两项：路径里有空格时加了引号，资源管理器也认得
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), path});
#endif
}

void FileTransferList::setTransfers(const QList<rc::Transfer> &transfers)
{
    if (transfers == list_)
        return;
    list_ = transfers;
    QVariantList others;
    QVariantList offers;
    for (const rc::Transfer &t : std::as_const(list_)) {
        if (!t.sending && t.status == transfer::Offered)
            offers.append(toQml(t));
        else
            others.append(toQml(t));
    }
    if (others != transfers_) {
        transfers_ = others;
        emit transfersChanged();
    }
    if (offers != offers_) {
        offers_ = offers;
        emit offersChanged();
    }
}

QString FileTransferList::localPaths(const QList<QUrl> &urls, QStringList &paths)
{
    if (urls.isEmpty())
        return tr("只能发送这台电脑上的文件和文件夹");
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            return tr("只能发送这台电脑上的文件和文件夹");
        // 文件夹的地址以 "/" 结尾：去掉，名字才是文件夹的名字
        paths.append(QDir::cleanPath(url.toLocalFile()));
    }
    return {};
}

const rc::Transfer *FileTransferList::find(int id) const
{
    const auto found = std::find_if(list_.cbegin(), list_.cend(), [id](const rc::Transfer &t) { return t.id == id; });
    return found == list_.cend() ? nullptr : &*found;
}

QVariantMap FileTransferList::toQml(const rc::Transfer &t)
{
    return {
        {QStringLiteral("id"), t.id},
        {QStringLiteral("viewer"), t.viewer},
        {QStringLiteral("sending"), t.sending},
        {QStringLiteral("name"), t.entries > 1 ? tr("%1 等 %2 项").arg(t.name).arg(t.entries) : t.name},
        {QStringLiteral("status"), t.status},
        {QStringLiteral("totalBytes"), t.totalBytes},
        {QStringLiteral("doneBytes"), t.doneBytes},
        {QStringLiteral("bytesPerSecond"), t.bytesPerSecond},
        {QStringLiteral("files"), t.files},
        {QStringLiteral("message"), message(t)},
        {QStringLiteral("folder"), folderName(t.directory)},
        {QStringLiteral("savedPath"), t.savedPath},
        {QStringLiteral("resumable"), t.resumable},
        {QStringLiteral("resumed"), t.resumed},
    };
}

// 结束了时给人看的说明（完成了的只说有几个没传成）
QString FileTransferList::message(const rc::Transfer &t)
{
    const QString &reason = t.reason;
    if (t.status == transfer::Completed)
        return t.missed > 0 ? tr("%1 个没传成").arg(t.missed) : QString();
    if (t.status == transfer::Rejected) {
        if (reason == transfer::TimedOut)
            return t.sending ? tr("对方一直没有接收") : tr("没有及时接收，已经过期了");
        if (reason == transfer::Unattended)
            return tr("对方电脑前没有人能接收");
        if (reason == transfer::Busy)
            return tr("对方还有很多文件没决定收不收，过一会儿再发");
        return tr("对方没有接收");
    }
    if (t.status == transfer::Cancelled)
        return reason == transfer::ByPeer ? tr("对方取消了") : tr("已取消");
    if (t.status != transfer::Failed)
        return {};
    if (reason == transfer::Disconnected && t.resumable)
        return t.sending ? tr("没发完，重试时接着发") : tr("没收完，对方再发时接着收");
    if (reason == transfer::Disconnected)
        return t.sending ? tr("连接断了，没发完") : tr("连接断了，没收完");
    if (reason == transfer::Unsupported)
        return tr("对方的版本不能互传文件，请对方更新");
    if (reason == transfer::Device)
        return t.sending ? tr("文件读不出来") : tr("存不下：磁盘空间不够，或者文件夹不能写入");
    if (reason == transfer::PeerDevice)
        return t.sending ? tr("对方存不下：磁盘空间不够，或者文件夹不能写入") : tr("对方的文件读不出来");
    if (reason == transfer::NotSendable)
        return tr("发不了：有的文件不在了、名字对方存不了，或者一次超过一万个");
    if (reason == transfer::Broken)
        return tr("对方的程序出错了");
    if (reason == transfer::Partial)
        return t.sending ? tr("一个都没发成") : tr("一个都没收成");
    return tr("传输出错了");
}

// 存到的文件夹给人看的名字：系统的"下载"和设置里显示的一样
QString FileTransferList::folderName(const QString &directory)
{
    if (directory.isEmpty())
        return {};
    if (QDir(directory) == QDir(downloads()))
        return tr("下载");
    return QDir(directory).dirName();
}
