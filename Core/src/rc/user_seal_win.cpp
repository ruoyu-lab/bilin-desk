#include "rc/user_seal.h"

#include <windows.h>

#include <dpapi.h>

namespace rc {

namespace {

// 和别的程序用 DPAPI 存的东西分开：别的程序拿着这份数据也解不开
constexpr char Entropy[] = "Bilin device secrets";

DATA_BLOB blob(const QByteArray &data)
{
    return {DWORD(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.constData()))};
}

QByteArray take(DATA_BLOB &blob)
{
    QByteArray data(reinterpret_cast<const char *>(blob.pbData), qsizetype(blob.cbData));
    LocalFree(blob.pbData);
    return data;
}

} // namespace

QByteArray sealForUser(const QByteArray &data)
{
    DATA_BLOB in = blob(data);
    DATA_BLOB entropy = blob(QByteArray::fromRawData(Entropy, sizeof Entropy - 1));
    DATA_BLOB out{};
    if (!CryptProtectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        qCritical("DPAPI 加密不了（%lu）", GetLastError());
        return {};
    }
    return take(out);
}

std::optional<QByteArray> unsealForUser(const QByteArray &sealed)
{
    DATA_BLOB in = blob(sealed);
    DATA_BLOB entropy = blob(QByteArray::fromRawData(Entropy, sizeof Entropy - 1));
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return std::nullopt;
    return take(out);
}

} // namespace rc
