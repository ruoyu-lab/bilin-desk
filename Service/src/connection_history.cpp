#include "connection_history.h"

#include <rc/json_file.h>

#include <QDateTime>
#include <QFileInfo>

#include <algorithm>

namespace protocol = rc::protocol;

namespace {

QString connectionKey(const QString &device, const QString &session)
{
    return device + u'/' + session;
}

} // namespace

ConnectionHistory::ConnectionHistory(QString file, std::function<qint64()> now, QObject *parent)
    : QObject(parent)
    , file_(std::move(file))
    , now_(now ? std::move(now) : [] { return QDateTime::currentMSecsSinceEpoch(); })
{
    load();
}

void ConnectionHistory::connected(const protocol::SharePeer &viewer, const QString &session)
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("connection");
    entry.deviceId = viewer.deviceId;
    entry.name = viewer.name;
    entry.control = viewer.control;
    open_.insert(connectionKey(viewer.deviceId, session), nextId_);
    add(std::move(entry));
}

void ConnectionHistory::gone(const QString &device, const QString &session)
{
    const int id = open_.take(connectionKey(device, session));
    const auto entry = std::find_if(entries_.begin(), entries_.end(), [id](const protocol::HistoryEntry &e) {
        return e.id == id;
    });
    // 早就挤出去了（连得太久，后来的记录太多）
    if (entry == entries_.end())
        return;
    entry->endedAt = now_();
    store();
    emit changed();
}

void ConnectionHistory::refused(const protocol::SharePeer &request, const QString &reason)
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("refused");
    entry.deviceId = request.deviceId;
    entry.name = request.name;
    entry.reason = reason;
    add(std::move(entry));
}

void ConnectionHistory::wrongPin()
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("wrongPin");
    add(std::move(entry));
}

void ConnectionHistory::updated(const QString &version)
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("updated");
    entry.version = version;
    add(std::move(entry));
}

void ConnectionHistory::power(const protocol::SharePeer &peer, const QString &action)
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("power");
    entry.deviceId = peer.deviceId;
    entry.name = peer.name;
    entry.action = action;
    add(std::move(entry));
}

void ConnectionHistory::transferred(const protocol::SharePeer &peer, const rc::Transfer &transfer)
{
    protocol::HistoryEntry entry;
    entry.kind = QStringLiteral("file");
    entry.deviceId = peer.deviceId;
    entry.name = peer.name;
    entry.file = transfer.name;
    entry.count = transfer.entries;
    entry.bytes = transfer.doneBytes;
    entry.sent = transfer.sending;
    add(std::move(entry));
}

void ConnectionHistory::clear()
{
    entries_.clear();
    open_.clear();
    store();
    emit changed();
}

void ConnectionHistory::add(protocol::HistoryEntry entry)
{
    entry.id = nextId_++;
    entry.at = now_();
    entries_.prepend(std::move(entry));
    if (entries_.size() > MaxEntries)
        entries_.resize(MaxEntries);
    store();
    emit changed();
}

void ConnectionHistory::load()
{
    entries_ = protocol::HistoryState::fromJson(rc::readJsonFile(file_)).entries;
    const qint64 lastWritten = QFileInfo(file_).lastModified().toMSecsSinceEpoch();
    for (protocol::HistoryEntry &entry : entries_) {
        nextId_ = std::max(nextId_, entry.id + 1);
        if (entry.kind == u"connection" && entry.endedAt == 0)
            entry.endedAt = std::max(lastWritten, entry.at);
    }
}

void ConnectionHistory::store()
{
    if (!rc::writeJsonFile(file_, state().toJson()))
        qWarning("连接记录存不下来：%s", qUtf8Printable(file_));
}
