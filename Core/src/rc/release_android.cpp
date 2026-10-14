#include "rc/release.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <memory>

namespace rc::release {

namespace {

template<auto Free>
struct Deleter
{
    template<typename T>
    void operator()(T *p) const { Free(p); }
};

using Key = std::unique_ptr<EVP_PKEY, Deleter<EVP_PKEY_free>>;
using KeyContext = std::unique_ptr<EVP_PKEY_CTX, Deleter<EVP_PKEY_CTX_free>>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, Deleter<EVP_MD_CTX_free>>;
using Signature = std::unique_ptr<ECDSA_SIG, Deleter<ECDSA_SIG_free>>;

// OPENSSL_free 是宏
void freeBytes(unsigned char *p)
{
    OPENSSL_free(p);
}
using Bytes = std::unique_ptr<unsigned char, Deleter<freeBytes>>;

// P-256 未压缩的点（65 字节）
Key publicKeyOf(const QByteArray &point)
{
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char *>("prime256v1"), 0),
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, const_cast<char *>(point.constData()),
                                          size_t(point.size())),
        OSSL_PARAM_construct_end(),
    };
    const KeyContext context(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
    EVP_PKEY *key = nullptr;
    if (!context || EVP_PKEY_fromdata_init(context.get()) != 1
        || EVP_PKEY_fromdata(context.get(), &key, EVP_PKEY_PUBLIC_KEY, params) != 1)
        return nullptr;
    return Key(key);
}

} // namespace

// Android：用 OpenSSL（跟着 APK 带着，Qt 的加密连接也用它；App/CMakeLists.txt）
bool signatureValid(const QByteArray &publicKey, const QByteArray &text, const QByteArray &signature)
{
    if (publicKey.size() != 65 || signature.size() != 64)
        return false;
    const Key key = publicKeyOf(publicKey);
    if (!key)
        return false;
    // r||s 换成 OpenSSL 要的 DER
    const auto bytes = reinterpret_cast<const unsigned char *>(signature.constData());
    const Signature sig(ECDSA_SIG_new());
    if (!sig || ECDSA_SIG_set0(sig.get(), BN_bin2bn(bytes, 32, nullptr), BN_bin2bn(bytes + 32, 32, nullptr)) != 1)
        return false;
    unsigned char *der = nullptr;
    const int derSize = i2d_ECDSA_SIG(sig.get(), &der);
    const Bytes derOwner(der);
    if (derSize <= 0)
        return false;
    const DigestContext context(EVP_MD_CTX_new());
    return context && EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1
           && EVP_DigestVerify(context.get(), der, size_t(derSize), reinterpret_cast<const unsigned char *>(text.constData()),
                               size_t(text.size()))
                  == 1;
}

} // namespace rc::release
