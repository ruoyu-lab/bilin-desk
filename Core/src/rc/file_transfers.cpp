#include "rc/file_transfers.h"

#include "rc/download_mark.h"

#include "rc/peer_protocol.h"

#include <rcsdk/rcsdk.hpp>

#include <QDir>

#include <algorithm>
#include <cstring>
#include <vector>

namespace rc {

namespace {

// rcs_file_get_items 一次取多少项（一项 2 KB 多）
constexpr uint32_t ItemsPage = 64;

QLatin1StringView statusOf(rcs_file_state state)
{
    switch (state) {
    case RCS_FILE_PREPARING: return transfer::Preparing;
    case RCS_FILE_OFFERED: return transfer::Offered;
    case RCS_FILE_PAUSED: return transfer::Paused;
    case RCS_FILE_WAITING: return transfer::Waiting;
    default: return transfer::Running;
    }
}

// 被拒绝：对方说的原因（rcs_file_reject 的 reason，本程序给的是 rc/peer_protocol.h 的），或者 SDK 替对方拒绝的
QLatin1StringView rejectedBecause(const rcs_file_done_event &done)
{
    switch (done.result) {
    case RCS_ERROR_TIMEOUT: return transfer::TimedOut;
    case RCS_ERROR_BUSY: return transfer::Busy;
    case RCS_ERROR_CANCELLED:
        return std::strcmp(done.message, rc::peer::Unattended) == 0 ? transfer::Unattended : transfer::Refused;
    default: return {};
    }
}

QLatin1StringView failedBecause(const rcs_file_done_event &done)
{
    switch (done.result) {
    case RCS_ERROR_DISCONNECTED: return transfer::Disconnected;
    case RCS_ERROR_DEVICE: return done.by_peer ? transfer::PeerDevice : transfer::Device;
    // 发的一端列出要发的东西时：不在了、名字对方存不了、超过上限
    case RCS_ERROR_INVALID_ARGUMENT: return transfer::NotSendable;
    case RCS_ERROR_PROTOCOL: return transfer::Broken;
    default: return {};
    }
}

const char *directionName(const Transfer &t)
{
    return t.sending ? "发" : "收";
}

QByteArray tokenOf(const uint8_t (&token)[RCS_FILE_TOKEN_SIZE])
{
    return QByteArray(reinterpret_cast<const char *>(token), RCS_FILE_TOKEN_SIZE);
}

} // namespace

FileTransfers::FileTransfers(InterruptedTransfers &interrupted, QObject *parent)
    : QObject(parent)
    , interrupted_(interrupted)
{
}

// 收的一端没收完的留着（模块默认就留），下次接着收
void FileTransfers::configure(rcsdk::SessionConfig &config)
{
    rcs_file_transfer_settings settings = rcs_file_transfer_settings_init();
    settings.flow = RCS_FILE_SEND | RCS_FILE_RECEIVE;
    config.useModule(RCS_FILE_TRANSFER_MODULE, settings);
}

std::optional<FileEvent> FileTransfers::read(const rcs_event &event)
{
    if (event.type != RCS_EVENT_MODULE || std::strcmp(event.module.module, RCS_FILE_TRANSFER_MODULE) != 0)
        return std::nullopt;
    FileEvent file;
    file.viewer = int(event.viewer_id);
    const void *payload = event.module.payload;
    switch (event.module.kind) {
    case RCS_MODULE_ACTIVE:
        file.what = FileEvent::Peer{true, {}};
        break;
    case RCS_MODULE_INACTIVE:
        file.what = FileEvent::Peer{false, QString::fromUtf8(event.module.message)};
        break;
    case RCS_FILE_OFFER:
        file.what = *static_cast<const rcs_file_offer_event *>(payload);
        break;
    case RCS_FILE_STATE:
        file.what = *static_cast<const rcs_file_state_event *>(payload);
        break;
    case RCS_FILE_PROGRESS:
        file.what = *static_cast<const rcs_file_progress_event *>(payload);
        break;
    case RCS_FILE_DONE:
        file.what = *static_cast<const rcs_file_done_event *>(payload);
        break;
    case RCS_FILE_DISCARDED:
        file.what = *static_cast<const rcs_file_discard_event *>(payload);
        break;
    default:
        // 每一项的结果（DONE 里有合计）、对方的设置
        return std::nullopt;
    }
    return file;
}

void FileTransfers::attach(rcs_session session)
{
    session_ = session;
}

void FileTransfers::detach()
{
    session_ = nullptr;
    unsupported_.clear();
    list_.detached();
    emit changed();
}

void FileTransfers::handle(const FileEvent &event)
{
    if (const auto *peer = std::get_if<FileEvent::Peer>(&event.what)) {
        if (peer->active) {
            unsupported_.remove(event.viewer);
            discardLeftovers();
        } else {
            qInfo("对方（%d）不能互传文件：%s", event.viewer, qUtf8Printable(peer->why));
            unsupported_.insert(event.viewer);
        }
        return;
    }
    if (const auto *discarded = std::get_if<rcs_file_discard_event>(&event.what)) {
        if (discarded->result == RCS_OK)
            qInfo("删掉了上次没收完的 %u 个半截文件", discarded->files);
        else
            qWarning("上次没收完的半截文件没删掉：%s", discarded->message);
        return;
    }
    if (const auto *offer = std::get_if<rcs_file_offer_event>(&event.what)) {
        offered(event.viewer, *offer);
    } else if (const auto *state = std::get_if<rcs_file_state_event>(&event.what)) {
        list_.setStatus(state->transfer, event.viewer, statusOf(state->state));
    } else if (const auto *progress = std::get_if<rcs_file_progress_event>(&event.what)) {
        list_.setProgress(progress->transfer, event.viewer, qint64(progress->done_bytes), qint64(progress->total_bytes),
                          qint64(progress->bytes_per_second));
    } else {
        finished(event.viewer, std::get<rcs_file_done_event>(event.what));
        // 不要了的那次传输结束了（比如拒绝了接着发来的），这才删得了它的半截文件
        discardLeftovers();
    }
    emit changed();
}

void FileTransfers::setAttended(bool attended)
{
    attended_ = attended;
    const QList<int> offers = list_.offers();
    if (attended_ || offers.isEmpty())
        return;
    for (const int id : offers) {
        rcs_file_reject(session_, list_.sdkId(id), rc::peer::Unattended);
        list_.remove(id);
    }
    qInfo("界面程序关了，对方发来、还没决定的 %lld 份文件都拒绝了", qlonglong(offers.size()));
    emit changed();
}

void FileTransfers::setPeer(int viewer, const QString &peer)
{
    peers_.insert(viewer, peer);
    for (const InterruptedTransfer &interrupted : interrupted_.forPeer(peer)) {
        // 还在传的由 SDK 跟着
        if (const Transfer *listed = list_.findToken(interrupted.token)) {
            if (listed->finished())
                list_.setViewer(listed->id, viewer);
        } else {
            list_.addInterrupted(viewer, interrupted);
        }
    }
    emit changed();
}

int FileTransfers::send(int viewer, const QStringList &paths)
{
    const int id = list_.addSending(viewer, paths);
    start(id);
    emit changed();
    return id;
}

void FileTransfers::start(int id)
{
    const Transfer sending = *list_.find(id);
    if (!session_) {
        list_.failed(id, transfer::Disconnected);
        return;
    }
    const QStringList paths = list_.paths(id);
    std::vector<QByteArray> utf8;
    for (const QString &path : paths)
        utf8.push_back(QDir::toNativeSeparators(path).toUtf8());
    std::vector<rcs_file_item> items(utf8.size());
    for (size_t i = 0; i < utf8.size(); ++i)
        items[i].path = utf8[i].constData();
    rcs_file_offer offer = rcs_file_offer_init();
    offer.viewer_id = rcs_viewer_id(sending.viewer);
    offer.count = uint32_t(items.size());
    offer.items = items.data();
    // 上次没发完的：同一个 token，对方接着收
    QByteArray token = list_.token(id);
    const bool resuming = !token.isEmpty();
    if (resuming)
        std::memcpy(offer.resume_token, token.constData(), RCS_FILE_TOKEN_SIZE);
    uint64_t sdk = 0;
    if (const rcs_result result = rcs_file_send(session_, &offer, &sdk); result != RCS_OK) {
        qWarning("发不出去 %s 等 %lld 项：%s", qUtf8Printable(sending.name), qlonglong(paths.size()),
                 rcsdk::lastCallError().c_str());
        // 参数不对只会是一次超过了上限；其余是没连着对方：对方的程序没有这个模块，或者刚好断了
        if (result == RCS_ERROR_INVALID_ARGUMENT)
            list_.failed(id, transfer::NotSendable);
        else
            list_.failed(id, unsupported_.contains(sending.viewer) ? transfer::Unsupported : transfer::Disconnected);
        return;
    }
    if (!resuming) {
        rcs_file_transfer_info info = rcs_file_transfer_info_init();
        if (rcs_file_get_transfer(session_, sdk, &info) == RCS_OK)
            token = tokenOf(info.token);
        else
            qWarning("查不到发出去的这次传输：%s", rcsdk::lastCallError().c_str());
    }
    qInfo("%s %s 等 %lld 项给对方（%d）", resuming ? "接着发" : "发", qUtf8Printable(sending.name), qlonglong(paths.size()),
          sending.viewer);
    list_.started(id, sdk, token);
    remember(id);
}

void FileTransfers::accept(int id, const QString &directory)
{
    // 已经结束了（刚好超时了、对方取消了）
    const quint64 sdk = list_.sdkId(id);
    if (!sdk)
        return;
    const QByteArray path = QDir::toNativeSeparators(directory).toUtf8();
    if (rcs_file_accept(session_, sdk, path.constData(), RCS_FILE_CONFLICT_RENAME) != RCS_OK) {
        qWarning("收不了：%s", rcsdk::lastCallError().c_str());
        return;
    }
    qInfo("收下对方发来的 %s，存到 %s", qUtf8Printable(list_.find(id)->name), qUtf8Printable(directory));
    list_.accepted(id, directory);
    remember(id);
    emit changed();
}

void FileTransfers::reject(int id)
{
    const quint64 sdk = list_.sdkId(id);
    if (!sdk || rcs_file_reject(session_, sdk, rc::peer::RefusedByUser) != RCS_OK)
        return;
    qInfo("拒绝了对方发来的 %s", qUtf8Printable(list_.find(id)->name));
    // 接着上次的也不要了：上次的半截文件删掉（这次结束以后）
    if (list_.find(id)->resumed)
        interrupted_.discard(list_.token(id));
    // 自己拒绝的不用留在列表里
    list_.remove(id);
    emit changed();
}

void FileTransfers::cancel(int id)
{
    // 结束了以后由 RCS_FILE_DONE 告诉
    if (const quint64 sdk = list_.sdkId(id); sdk && rcs_file_cancel(session_, sdk) != RCS_OK)
        qWarning("取消不了：%s", rcsdk::lastCallError().c_str());
}

void FileTransfers::retry(int id)
{
    if (!list_.restart(id))
        return;
    start(id);
    emit changed();
}

void FileTransfers::remove(int id)
{
    const Transfer *removed = list_.find(id);
    if (!removed)
        return;
    // 没传完、留着下次接着传的不要了：收的删掉半截文件
    if (removed->resumable) {
        interrupted_.discard(list_.token(id));
        discardLeftovers();
    }
    list_.remove(id);
    emit changed();
}

void FileTransfers::clearCompleted()
{
    list_.clearCompleted();
    emit changed();
}

void FileTransfers::clear()
{
    list_.clear();
    emit changed();
}

void FileTransfers::offered(int viewer, const rcs_file_offer_event &offer)
{
    if (!attended_) {
        qInfo("对方（%d）发来 %u 项，没人能决定收不收，拒绝", viewer, offer.items);
        rcs_file_reject(session_, offer.transfer, rc::peer::Unattended);
        return;
    }
    const QByteArray token = tokenOf(offer.token);
    // 对方接着发上次没传完的：存回上次的文件夹，列表里上次的那一项换成这一个
    std::optional<InterruptedTransfer> earlier;
    if (const InterruptedTransfer *found = offer.resuming ? interrupted_.find(token) : nullptr;
        found && !found->sending && !found->discard && found->peer == peers_.value(viewer))
        earlier = *found;
    if (const Transfer *listed = list_.findToken(token); listed && listed->finished())
        list_.remove(listed->id);
    const int id = list_.offered(offer.transfer, viewer, items(offer.transfer), qint64(offer.total_bytes), token);
    if (earlier)
        list_.resuming(id, earlier->directory, earlier->doneBytes);
    qInfo("对方（%d）%s %u 项（%llu 字节），等本机的人决定", viewer, earlier ? "接着发来上次没传完的" : "发来",
          offer.items, static_cast<unsigned long long>(offer.total_bytes));
}

void FileTransfers::finished(int viewer, const rcs_file_done_event &done)
{
    const Transfer *ended = list_.findSdk(done.transfer);
    // 本端拒绝了、已经去掉了的
    if (!ended)
        return;
    const int missed = int(done.items_failed + done.items_skipped);
    QLatin1StringView status;
    QLatin1StringView reason;
    switch (done.end) {
    case RCS_FILE_COMPLETED:
        status = done.items_completed == 0 && missed > 0 ? transfer::Failed : transfer::Completed;
        reason = missed > 0 ? transfer::Partial : QLatin1StringView();
        break;
    case RCS_FILE_REJECTED:
        status = transfer::Rejected;
        reason = rejectedBecause(done);
        break;
    case RCS_FILE_CANCELLED:
        status = transfer::Cancelled;
        reason = done.by_peer ? transfer::ByPeer : QLatin1StringView();
        break;
    default:
        status = transfer::Failed;
        reason = failedBecause(done);
        break;
    }
    if (done.end == RCS_FILE_COMPLETED)
        qInfo("%s完了 %s：%u 个成了，%d 个没成", directionName(*ended), qUtf8Printable(ended->name),
              done.items_completed, missed);
    else
        qInfo("%s %s 没完成（%s）：%s", directionName(*ended), qUtf8Printable(ended->name), qUtf8Printable(QString(status)),
              done.message);
    const bool received = !ended->sending && done.end == RCS_FILE_COMPLETED;
    if (received)
        markReceived(done.transfer);
    const QString saved = received ? savedPath(done.transfer) : QString();
    // 连接断了、会话停了：发的下次接着发；收的留着半截文件时下次接着收
    const bool resumable = done.end == RCS_FILE_FAILED
                           && (ended->sending ? done.result == RCS_ERROR_DISCONNECTED : done.partial_kept != 0);
    const QByteArray token = list_.token(ended->id);
    const QStringList paths = list_.paths(ended->id);
    const std::optional<Transfer> finishedTransfer =
        list_.finish(done.transfer, viewer, status, reason, missed, qint64(done.done_bytes), saved, resumable);
    if (resumable)
        remember(*finishedTransfer, token, paths);
    else
        forget(token);
    if (status == transfer::Completed)
        emit completed(*finishedTransfer);
}

// 对方发来的各项（文件夹里的跟在文件夹后面）
QList<TransferList::Item> FileTransfers::items(quint64 transfer) const
{
    QList<TransferList::Item> items;
    std::vector<rcs_file_item_info> page(ItemsPage);
    for (uint32_t first = 0;;) {
        uint32_t count = 0;
        const rcs_result result = rcs_file_get_items(session_, transfer, first, page.data(), sizeof(rcs_file_item_info),
                                                     ItemsPage, &count);
        if (result != RCS_OK && result != RCS_ERROR_BUFFER_TOO_SMALL) {
            qWarning("列不出对方发来的：%s", rcsdk::lastCallError().c_str());
            return items;
        }
        for (uint32_t k = 0; k < std::min(count, ItemsPage); ++k)
            items.append({QString::fromUtf8(page[k].path), page[k].kind == RCS_FILE_REGULAR});
        if (count <= ItemsPage)
            return items;
        first += ItemsPage;
    }
}

// 收完了：收成的文件都标上"来自网络"
void FileTransfers::markReceived(quint64 transfer) const
{
    const QString directory = list_.findSdk(transfer)->directory;
    std::vector<rcs_file_item_info> page(ItemsPage);
    for (uint32_t first = 0;;) {
        uint32_t count = 0;
        const rcs_result result = rcs_file_get_items(session_, transfer, first, page.data(), sizeof(rcs_file_item_info),
                                                     ItemsPage, &count);
        if (result != RCS_OK && result != RCS_ERROR_BUFFER_TOO_SMALL) {
            qWarning("列不出收到的：%s", rcsdk::lastCallError().c_str());
            return;
        }
        for (uint32_t k = 0; k < std::min(count, ItemsPage); ++k) {
            if (page[k].kind == RCS_FILE_REGULAR && page[k].state == RCS_FILE_ITEM_COMPLETED)
                markDownloaded(QDir::cleanPath(directory + u'/' + QString::fromUtf8(page[k].saved_as)));
        }
        if (count <= ItemsPage)
            return;
        first += ItemsPage;
    }
}

void FileTransfers::remember(int id)
{
    if (const Transfer *t = list_.find(id))
        remember(*t, list_.token(id), list_.paths(id));
}

void FileTransfers::remember(const Transfer &transfer, const QByteArray &token, const QStringList &paths)
{
    const QString peer = peers_.value(transfer.viewer);
    if (peer.isEmpty() || token.isEmpty())
        return;
    InterruptedTransfer interrupted;
    interrupted.token = token;
    interrupted.peer = peer;
    interrupted.sending = transfer.sending;
    interrupted.name = transfer.name;
    interrupted.entries = transfer.entries;
    interrupted.files = transfer.files;
    interrupted.totalBytes = transfer.totalBytes;
    interrupted.doneBytes = transfer.doneBytes;
    interrupted.paths = paths;
    interrupted.directory = transfer.directory;
    interrupted.at = QDateTime::currentDateTimeUtc();
    interrupted_.put(interrupted);
}

// 不要了的（等着删半截文件）留着
void FileTransfers::forget(const QByteArray &token)
{
    if (const InterruptedTransfer *interrupted = interrupted_.find(token); interrupted && !interrupted->discard)
        interrupted_.remove(token);
}

void FileTransfers::discardLeftovers()
{
    if (!session_)
        return;
    for (const InterruptedTransfer &leftover : interrupted_.discarded()) {
        const QByteArray directory = QDir::toNativeSeparators(leftover.directory).toUtf8();
        // 模块还没在本机跑起来、那次传输还没结束：下次再删
        if (rcs_file_discard(session_, directory.constData(), reinterpret_cast<const uint8_t *>(leftover.token.constData()))
            == RCS_OK)
            interrupted_.remove(leftover.token);
    }
}

// 收完了：第一项存成了哪里（重名时改了名）；它没收成就是存到的文件夹
QString FileTransfers::savedPath(quint64 transfer) const
{
    const QString directory = list_.findSdk(transfer)->directory;
    rcs_file_item_info first{};
    uint32_t count = 0;
    const rcs_result result = rcs_file_get_items(session_, transfer, 0, &first, sizeof first, 1, &count);
    if ((result != RCS_OK && result != RCS_ERROR_BUFFER_TOO_SMALL) || first.state != RCS_FILE_ITEM_COMPLETED)
        return directory;
    return QDir::cleanPath(directory + u'/' + QString::fromUtf8(first.saved_as));
}

} // namespace rc
