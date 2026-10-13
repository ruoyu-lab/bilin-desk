#include "rc/temporary_password.h"

#include <QRandomGenerator>

namespace rc {

QString generateTemporaryPassword()
{
    constexpr int alphabetSize = int(sizeof(TemporaryPasswordAlphabet)) - 1;
    QString password;
    password.reserve(TemporaryPasswordLength);
    for (qsizetype i = 0; i < TemporaryPasswordLength; ++i)
        password.append(QLatin1Char(TemporaryPasswordAlphabet[QRandomGenerator::system()->bounded(alphabetSize)]));
    return password;
}

} // namespace rc
