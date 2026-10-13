#include "rc/release.h"

#include <QCryptographicHash>

#include <windows.h>

#include <bcrypt.h>

#include <vector>

namespace rc::release {

bool signatureValid(const QByteArray &publicKey, const QByteArray &text, const QByteArray &signature)
{
    if (publicKey.size() != 65 || signature.size() != 64)
        return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0)))
        return false;
    // BCRYPT_ECCKEY_BLOB 后面跟着 X、Y（各 32 字节）：去掉未压缩的点前面的 0x04
    std::vector<UCHAR> blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64);
    auto *header = reinterpret_cast<BCRYPT_ECCKEY_BLOB *>(blob.data());
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = 32;
    memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), publicKey.constData() + 1, 64);
    BCRYPT_KEY_HANDLE key = nullptr;
    bool valid = false;
    if (BCRYPT_SUCCESS(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), ULONG(blob.size()), 0))) {
        // BCrypt 验的是哈希，签名是 r||s
        QByteArray hash = QCryptographicHash::hash(text, QCryptographicHash::Sha256);
        QByteArray raw = signature;
        valid = BCRYPT_SUCCESS(BCryptVerifySignature(key, nullptr, reinterpret_cast<PUCHAR>(hash.data()), ULONG(hash.size()),
                                                     reinterpret_cast<PUCHAR>(raw.data()), ULONG(raw.size()), 0));
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return valid;
}

} // namespace rc::release
