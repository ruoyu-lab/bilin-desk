#include "share_transport.h"

#include <algorithm>

ShareTransport::ShareTransport(int attemptMs)
    : attemptMs_(attemptMs)
{
}

ShareTransport::Verdict ShareTransport::check(const QString &from, const QString &wanted, bool inUse, bool hasRelay)
{
    if (wanted == rc::RelayTransport && !hasRelay)
        return Verdict::NoRelay;
    if (wanted == current_)
        return Verdict::Accept;
    if (inUse || othersAttempting(from))
        return Verdict::Busy;
    return Verdict::Switch;
}

void ShareTransport::attempting(const QString &from)
{
    attempts_.insert(from, QDeadlineTimer(attemptMs_));
}

void ShareTransport::arrived(const QString &from)
{
    attempts_.remove(from);
}

void ShareTransport::switchTo(const QString &transport)
{
    current_ = transport;
    attempts_.clear();
}

bool ShareTransport::shouldSwitchBack(bool inUse)
{
    forgetAttempts();
    return current_ == rc::RelayTransport && !inUse && attempts_.isEmpty();
}

bool ShareTransport::othersAttempting(const QString &from)
{
    forgetAttempts();
    return std::any_of(attempts_.keyBegin(), attempts_.keyEnd(), [&from](const QString &device) { return device != from; });
}

void ShareTransport::forgetAttempts()
{
    attempts_.removeIf([](const auto &entry) { return entry.value().hasExpired(); });
}
