#include "rc/interrupted_transfers.h"

#include "rc/json_file.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace rc {

namespace {

QJsonObject toJson(const InterruptedTransfer &t)
{
    QJsonObject json{
        {QStringLiteral("token"), QString::fromLatin1(t.token.toHex())},
        {QStringLiteral("peer"), t.peer},
        {QStringLiteral("sending"), t.sending},
        {QStringLiteral("name"), t.name},
        {QStringLiteral("entries"), t.entries},
        {QStringLiteral("files"), t.files},
        {QStringLiteral("totalBytes"), t.totalBytes},
        {QStringLiteral("doneBytes"), t.doneBytes},
        {QStringLiteral("at"), t.at.toString(Qt::ISODateWithMs)},
    };
    if (t.sending) {
        json.insert(QStringLiteral("paths"), QJsonArray::fromStringList(t.paths));
    } else {
        json.insert(QStringLiteral("directory"), t.directory);
        json.insert(QStringLiteral("discard"), t.discard);
    }
    return json;
}

InterruptedTransfer fromJson(const QJsonObject &json)
{
    InterruptedTransfer t;
    t.token = QByteArray::fromHex(json.value(QLatin1StringView("token")).toString().toLatin1());
    t.peer = json.value(QLatin1StringView("peer")).toString();
    t.sending = json.value(QLatin1StringView("sending")).toBool();
    t.name = json.value(QLatin1StringView("name")).toString();
    t.entries = json.value(QLatin1StringView("entries")).toInt();
    t.files = json.value(QLatin1StringView("files")).toInt();
    t.totalBytes = json.value(QLatin1StringView("totalBytes")).toInteger();
    t.doneBytes = json.value(QLatin1StringView("doneBytes")).toInteger();
    for (const QJsonValue &path : json.value(QLatin1StringView("paths")).toArray())
        t.paths.append(path.toString());
    t.directory = json.value(QLatin1StringView("directory")).toString();
    t.at = QDateTime::fromString(json.value(QLatin1StringView("at")).toString(), Qt::ISODateWithMs);
    t.discard = json.value(QLatin1StringView("discard")).toBool();
    return t;
}

template<typename List>
auto withToken(List &transfers, const QByteArray &token)
{
    return std::find_if(transfers.begin(), transfers.end(), [&token](const auto &t) { return t.token == token; });
}

} // namespace

InterruptedTransfers::InterruptedTransfers(QString file, const QDateTime &now)
    : file_(std::move(file))
{
    for (const QJsonValue &value : readJsonFile(file_).value(QLatin1StringView("transfers")).toArray())
        transfers_.append(fromJson(value.toObject()));
    // 放太久的：发的去掉，收的当作不要了
    const QDateTime oldest = now.addDays(-KeepDays);
    bool changed = false;
    for (auto it = transfers_.begin(); it != transfers_.end();) {
        if (it->at >= oldest || it->discard) {
            ++it;
        } else if (it->sending) {
            it = transfers_.erase(it);
            changed = true;
        } else {
            it->discard = true;
            changed = true;
            ++it;
        }
    }
    if (changed)
        save();
}

QList<InterruptedTransfer> InterruptedTransfers::forPeer(const QString &peer) const
{
    QList<InterruptedTransfer> found;
    for (const InterruptedTransfer &t : transfers_) {
        if (t.peer == peer && !t.discard)
            found.append(t);
    }
    return found;
}

const InterruptedTransfer *InterruptedTransfers::find(const QByteArray &token) const
{
    const auto found = withToken(transfers_, token);
    return found == transfers_.cend() ? nullptr : &*found;
}

void InterruptedTransfers::put(const InterruptedTransfer &transfer)
{
    const auto found = withToken(transfers_, transfer.token);
    if (found == transfers_.end())
        transfers_.append(transfer);
    else if (*found == transfer)
        return;
    else
        *found = transfer;
    save();
}

void InterruptedTransfers::remove(const QByteArray &token)
{
    if (transfers_.removeIf([&token](const InterruptedTransfer &t) { return t.token == token; }) > 0)
        save();
}

void InterruptedTransfers::discard(const QByteArray &token)
{
    const auto found = withToken(transfers_, token);
    if (found == transfers_.end())
        return;
    if (found->sending)
        transfers_.erase(found);
    else
        found->discard = true;
    save();
}

QList<InterruptedTransfer> InterruptedTransfers::discarded() const
{
    QList<InterruptedTransfer> found;
    for (const InterruptedTransfer &t : transfers_) {
        if (t.discard)
            found.append(t);
    }
    return found;
}

void InterruptedTransfers::save() const
{
    QJsonArray array;
    for (const InterruptedTransfer &t : transfers_)
        array.append(toJson(t));
    writeJsonFile(file_, {{QStringLiteral("transfers"), array}});
}

} // namespace rc
