#include "rc/user_seal.h"

namespace rc {

QByteArray sealForUser(const QByteArray &data)
{
    return data;
}

std::optional<QByteArray> unsealForUser(const QByteArray &sealed)
{
    return sealed;
}

} // namespace rc
