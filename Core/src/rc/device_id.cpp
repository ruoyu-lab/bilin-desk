#include "rc/device_id.h"

#include <QRandomGenerator>

namespace rc {

namespace {

constexpr qsizetype Length = 9;

} // namespace

DeviceId::DeviceId(QString digits)
    : digits_(std::move(digits))
{
}

DeviceId DeviceId::generate()
{
    return DeviceId(QString::number(QRandomGenerator::system()->bounded(100000000u, 1000000000u)));
}

DeviceId DeviceId::parse(QStringView text)
{
    QString digits;
    for (const QChar c : text) {
        if (c.isSpace() || c == u'-')
            continue;
        if (c < u'0' || c > u'9')
            return {};
        digits.append(c);
    }
    if (digits.size() != Length || digits.front() == u'0')
        return {};
    return DeviceId(std::move(digits));
}

QString DeviceId::display() const
{
    if (!isValid())
        return {};
    return digits_.first(3) + u' ' + digits_.sliced(3, 3) + u' ' + digits_.sliced(6);
}

} // namespace rc
