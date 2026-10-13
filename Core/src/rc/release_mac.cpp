#include "rc/release.h"

#include <Security/Security.h>

namespace rc::release {

bool signatureValid(const QByteArray &publicKey, const QByteArray &text, const QByteArray &signature)
{
    if (publicKey.size() != 65 || signature.size() != 64)
        return false;
    const void *keys[] = {kSecAttrKeyType, kSecAttrKeyClass};
    const void *values[] = {kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic};
    CFDictionaryRef attributes = CFDictionaryCreate(nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks);
    CFDataRef keyData = publicKey.toRawCFData();
    SecKeyRef key = SecKeyCreateWithData(keyData, attributes, nullptr);
    CFRelease(attributes);
    if (!key)
        return false;
    CFDataRef message = text.toRawCFData();
    CFDataRef raw = signature.toRawCFData();
    // RFC 4754：签名是 r||s（不是 DER）
    const bool valid = SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA256, message, raw, nullptr);
    CFRelease(raw);
    CFRelease(message);
    CFRelease(key);
    return valid;
}

} // namespace rc::release
