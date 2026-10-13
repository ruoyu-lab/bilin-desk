#include "incoming_history.h"

#include "service_client.h"

#include <rc/device_id.h>
#include <rc/protocol.h>

#include <QDateTime>
#include <QSettings>

namespace protocol = rc::protocol;

namespace {

// 偏好设置里记着看过到哪一条
constexpr QLatin1StringView SeenKey("historySeen");

} // namespace

IncomingHistory::IncomingHistory(ServiceClient &service, QObject *parent)
    : QObject(parent)
    , service_(service)
    , seen_(QSettings().value(SeenKey).toInt())
{
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic == protocol::HistoryTopic)
            update();
    });
}

int IncomingHistory::unread() const
{
    int count = 0;
    for (const QVariant &entry : entries_) {
        if (entry.toMap().value(QStringLiteral("id")).toInt() > seen_)
            ++count;
    }
    return count;
}

void IncomingHistory::markSeen()
{
    if (seen_ >= newest_)
        return;
    seen_ = newest_;
    QSettings().setValue(SeenKey, seen_);
    emit changed();
}

void IncomingHistory::clear()
{
    service_.send(protocol::ClearHistory);
}

void IncomingHistory::update()
{
    const auto state = protocol::HistoryState::fromJson(service_.topic(protocol::HistoryTopic));
    entries_.clear();
    for (const protocol::HistoryEntry &entry : state.entries) {
        entries_.append(QVariantMap{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("kind"), entry.kind},
            {QStringLiteral("deviceId"), rc::DeviceId::parse(entry.deviceId).display()},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("at"), QDateTime::fromMSecsSinceEpoch(entry.at)},
            {QStringLiteral("endedAt"), entry.endedAt > 0 ? QVariant(QDateTime::fromMSecsSinceEpoch(entry.endedAt)) : QVariant()},
            {QStringLiteral("control"), entry.control},
            {QStringLiteral("reason"), entry.reason},
            {QStringLiteral("file"), entry.file},
            {QStringLiteral("count"), entry.count},
            {QStringLiteral("bytes"), entry.bytes},
            {QStringLiteral("sent"), entry.sent},
            {QStringLiteral("version"), entry.version},
            {QStringLiteral("action"), entry.action},
        });
    }
    newest_ = state.entries.isEmpty() ? 0 : state.entries.first().id;
    // 清空过、换了一台电脑的记录：编号从头来了
    if (seen_ > newest_) {
        seen_ = newest_;
        QSettings().setValue(SeenKey, seen_);
    }
    emit changed();
}
