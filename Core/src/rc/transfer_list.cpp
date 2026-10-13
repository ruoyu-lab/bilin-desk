#include "rc/transfer_list.h"

#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>

namespace rc {

namespace {

template<typename Entries>
auto withId(Entries &entries, int id)
{
    return std::find_if(entries.begin(), entries.end(), [id](const auto &e) { return e.transfer.id == id; });
}

// SDK 的编号 0 是"不在 SDK 里"，不对应哪一个
template<typename Entries>
auto withSdk(Entries &entries, quint64 sdk)
{
    return sdk == 0 ? entries.end()
                    : std::find_if(entries.begin(), entries.end(), [sdk](const auto &e) { return e.sdk == sdk; });
}

} // namespace

bool Transfer::finished() const
{
    return status == transfer::Completed || status == transfer::Rejected || status == transfer::Cancelled
           || status == transfer::Failed;
}

QJsonObject Transfer::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("viewer"), viewer},
        {QStringLiteral("sending"), sending},
        {QStringLiteral("name"), name},
        {QStringLiteral("entries"), entries},
        {QStringLiteral("files"), files},
        {QStringLiteral("totalBytes"), totalBytes},
        {QStringLiteral("doneBytes"), doneBytes},
        {QStringLiteral("bytesPerSecond"), bytesPerSecond},
        {QStringLiteral("status"), status},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("missed"), missed},
        {QStringLiteral("directory"), directory},
        {QStringLiteral("savedPath"), savedPath},
        {QStringLiteral("resumable"), resumable},
        {QStringLiteral("resumed"), resumed},
    };
}

Transfer Transfer::fromJson(const QJsonObject &json)
{
    Transfer transfer;
    transfer.id = json.value(QLatin1StringView("id")).toInt();
    transfer.viewer = json.value(QLatin1StringView("viewer")).toInt();
    transfer.sending = json.value(QLatin1StringView("sending")).toBool();
    transfer.name = json.value(QLatin1StringView("name")).toString();
    transfer.entries = json.value(QLatin1StringView("entries")).toInt();
    transfer.files = json.value(QLatin1StringView("files")).toInt();
    transfer.totalBytes = json.value(QLatin1StringView("totalBytes")).toInteger();
    transfer.doneBytes = json.value(QLatin1StringView("doneBytes")).toInteger();
    transfer.bytesPerSecond = json.value(QLatin1StringView("bytesPerSecond")).toInteger();
    transfer.status = json.value(QLatin1StringView("status")).toString();
    transfer.reason = json.value(QLatin1StringView("reason")).toString();
    transfer.missed = json.value(QLatin1StringView("missed")).toInt();
    transfer.directory = json.value(QLatin1StringView("directory")).toString();
    transfer.savedPath = json.value(QLatin1StringView("savedPath")).toString();
    transfer.resumable = json.value(QLatin1StringView("resumable")).toBool();
    transfer.resumed = json.value(QLatin1StringView("resumed")).toBool();
    return transfer;
}

QJsonObject transfersToJson(const QList<Transfer> &transfers)
{
    QJsonArray array;
    for (const Transfer &transfer : transfers)
        array.append(transfer.toJson());
    return {{QStringLiteral("transfers"), array}};
}

QList<Transfer> transfersFromJson(const QJsonObject &json)
{
    QList<Transfer> transfers;
    for (const QJsonValue &transfer : json.value(QLatin1StringView("transfers")).toArray())
        transfers.append(Transfer::fromJson(transfer.toObject()));
    return transfers;
}

QList<Transfer> TransferList::all() const
{
    QList<Transfer> transfers;
    transfers.reserve(entries_.size());
    for (const Entry &entry : entries_)
        transfers.append(entry.transfer);
    return transfers;
}

const Transfer *TransferList::find(int id) const
{
    const auto found = withId(entries_, id);
    return found == entries_.cend() ? nullptr : &found->transfer;
}

const Transfer *TransferList::findSdk(quint64 sdk) const
{
    const auto found = withSdk(entries_, sdk);
    return found == entries_.cend() ? nullptr : &found->transfer;
}

quint64 TransferList::sdkId(int id) const
{
    const auto found = withId(entries_, id);
    return found == entries_.cend() ? 0 : found->sdk;
}

QStringList TransferList::paths(int id) const
{
    const auto found = withId(entries_, id);
    return found == entries_.cend() ? QStringList() : found->paths;
}

QList<int> TransferList::offers() const
{
    QList<int> ids;
    for (const Entry &entry : entries_) {
        if (!entry.transfer.sending && entry.transfer.status == transfer::Offered)
            ids.append(entry.transfer.id);
    }
    return ids;
}

QByteArray TransferList::token(int id) const
{
    const auto found = withId(entries_, id);
    return found == entries_.cend() ? QByteArray() : found->token;
}

const Transfer *TransferList::findToken(const QByteArray &token) const
{
    const auto found = std::find_if(entries_.cbegin(), entries_.cend(),
                                    [&token](const Entry &e) { return !token.isEmpty() && e.token == token; });
    return found == entries_.cend() ? nullptr : &found->transfer;
}

int TransferList::addSending(int viewer, const QStringList &paths)
{
    Entry entry;
    entry.transfer.id = nextId_++;
    entry.transfer.viewer = viewer;
    entry.transfer.sending = true;
    // 根目录没有名字，就用它本身
    const QString first = QFileInfo(paths.first()).fileName();
    entry.transfer.name = first.isEmpty() ? paths.first() : first;
    entry.transfer.entries = int(paths.size());
    entry.transfer.status = transfer::Preparing;
    entry.paths = paths;
    entries_.append(entry);
    return entry.transfer.id;
}

int TransferList::addInterrupted(int viewer, const InterruptedTransfer &interrupted)
{
    Entry entry;
    entry.token = interrupted.token;
    entry.paths = interrupted.paths;
    Transfer &t = entry.transfer;
    t.id = nextId_++;
    t.viewer = viewer;
    t.sending = interrupted.sending;
    t.name = interrupted.name;
    t.entries = interrupted.entries;
    t.files = interrupted.files;
    t.totalBytes = interrupted.totalBytes;
    t.doneBytes = interrupted.doneBytes;
    t.status = transfer::Failed;
    t.reason = transfer::Disconnected;
    t.directory = interrupted.directory;
    t.resumable = true;
    entries_.append(entry);
    prune();
    return t.id;
}

void TransferList::started(int id, quint64 sdk, const QByteArray &token)
{
    if (Entry *e = entry(id)) {
        e->sdk = sdk;
        e->token = token;
    }
}

void TransferList::failed(int id, QLatin1StringView reason)
{
    Entry *e = entry(id);
    if (!e)
        return;
    e->transfer.status = transfer::Failed;
    e->transfer.reason = reason;
    end(*e);
    prune();
}

bool TransferList::restart(int id)
{
    Entry *e = entry(id);
    if (!e || !e->transfer.sending || !e->transfer.finished() || e->transfer.status == transfer::Completed)
        return false;
    Transfer &t = e->transfer;
    t.status = transfer::Preparing;
    t.reason.clear();
    t.totalBytes = 0;
    t.doneBytes = 0;
    t.missed = 0;
    t.resumable = false;
    // 排到最后：它是最新的一次
    entries_.move(withId(entries_, id) - entries_.begin(), entries_.size() - 1);
    return true;
}

int TransferList::offered(quint64 sdk, int viewer, const QList<Item> &items, qint64 totalBytes, const QByteArray &token)
{
    Entry entry;
    entry.sdk = sdk;
    entry.token = token;
    Transfer &t = entry.transfer;
    t.id = nextId_++;
    t.viewer = viewer;
    t.sending = false;
    // 第一项一定在最上层：文件夹里的排在文件夹后面
    t.name = items.isEmpty() ? QString() : items.first().path;
    for (const Item &item : items) {
        if (!item.path.contains(u'/'))
            ++t.entries;
        if (item.file)
            ++t.files;
    }
    t.totalBytes = totalBytes;
    t.status = transfer::Offered;
    entries_.append(entry);
    return t.id;
}

void TransferList::resuming(int id, const QString &directory, qint64 doneBytes)
{
    Entry *e = entry(id);
    if (!e)
        return;
    e->transfer.resumed = true;
    e->transfer.directory = directory;
    e->transfer.doneBytes = doneBytes;
}

void TransferList::accepted(int id, const QString &directory)
{
    Entry *e = entry(id);
    if (!e)
        return;
    e->transfer.status = transfer::Preparing;
    e->transfer.directory = directory;
}

void TransferList::setStatus(quint64 sdk, int viewer, QLatin1StringView status)
{
    Entry *e = entrySdk(sdk);
    if (!e)
        return;
    e->transfer.viewer = viewer;
    e->transfer.status = status;
    if (status != transfer::Running)
        e->transfer.bytesPerSecond = 0;
}

void TransferList::setProgress(quint64 sdk, int viewer, qint64 doneBytes, qint64 totalBytes, qint64 bytesPerSecond)
{
    Entry *e = entrySdk(sdk);
    if (!e)
        return;
    e->transfer.viewer = viewer;
    e->transfer.doneBytes = doneBytes;
    e->transfer.totalBytes = totalBytes;
    e->transfer.bytesPerSecond = bytesPerSecond;
}

std::optional<Transfer> TransferList::finish(quint64 sdk, int viewer, QLatin1StringView status,
                                             QLatin1StringView reason, int missed, qint64 doneBytes,
                                             const QString &savedPath, bool resumable)
{
    Entry *e = entrySdk(sdk);
    if (!e)
        return std::nullopt;
    Transfer &t = e->transfer;
    t.viewer = viewer;
    t.status = status;
    t.reason = reason;
    t.missed = missed;
    t.doneBytes = doneBytes;
    t.savedPath = savedPath;
    t.resumable = resumable;
    end(*e);
    const Transfer finished = t;
    prune();
    return finished;
}

void TransferList::setViewer(int id, int viewer)
{
    if (Entry *e = entry(id))
        e->transfer.viewer = viewer;
}

void TransferList::detached()
{
    entries_.removeIf([](const Entry &e) { return !e.transfer.sending && e.transfer.status == transfer::Offered; });
    for (Entry &e : entries_) {
        if (!e.transfer.finished()) {
            e.transfer.status = transfer::Failed;
            e.transfer.reason = transfer::Disconnected;
            // 已经开始传的（SDK 给了 token）记着，下次接着传
            e.transfer.resumable = !e.token.isEmpty();
        }
        end(e);
    }
    prune();
}

void TransferList::remove(int id)
{
    entries_.removeIf([id](const Entry &e) { return e.transfer.id == id; });
}

void TransferList::clearCompleted()
{
    entries_.removeIf([](const Entry &e) { return e.transfer.status == transfer::Completed; });
}

void TransferList::clear()
{
    entries_.clear();
}

TransferList::Entry *TransferList::entry(int id)
{
    const auto found = withId(entries_, id);
    return found == entries_.end() ? nullptr : &*found;
}

TransferList::Entry *TransferList::entrySdk(quint64 sdk)
{
    const auto found = withSdk(entries_, sdk);
    return found == entries_.end() ? nullptr : &*found;
}

// 结束了：SDK 不再管它
void TransferList::end(Entry &entry)
{
    entry.sdk = 0;
    entry.transfer.bytesPerSecond = 0;
}

void TransferList::prune()
{
    qsizetype finished = std::count_if(entries_.cbegin(), entries_.cend(), [](const Entry &e) { return e.transfer.finished(); });
    for (auto it = entries_.begin(); finished > KeptFinished && it != entries_.end();) {
        if (it->transfer.finished()) {
            it = entries_.erase(it);
            --finished;
        } else {
            ++it;
        }
    }
}

} // namespace rc
