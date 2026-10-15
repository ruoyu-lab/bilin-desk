#include "rc/machine.h"

#include <QUuid>

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace rc {

// iOS 不给硬件编号（identifierForVendor 在本机没有我们别的程序时，删掉重装就变）：第一次用时生成一个，存在钥匙串里。
// 删掉程序钥匙串里的还在，重装以后不变；只在这台设备上（不随 iCloud 钥匙串、备份到别的设备）
QString machineId()
{
    @autoreleasepool {
        NSDictionary *const item = @{
            (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService: @"com.bilindesk.bilin.machine",
            (__bridge id)kSecAttrAccount: @"machine",
        };
        NSMutableDictionary *const query = [[item mutableCopy] autorelease];
        query[(__bridge id)kSecReturnData] = @YES;
        CFTypeRef found = nullptr;
        if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &found) == errSecSuccess) {
            const QString stored = QString::fromUtf8(QByteArray::fromNSData((__bridge NSData *)found));
            CFRelease(found);
            return stored;
        }

        const QByteArray generated = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        NSMutableDictionary *const added = [[item mutableCopy] autorelease];
        added[(__bridge id)kSecValueData] = generated.toNSData();
        added[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
        if (SecItemAdd((__bridge CFDictionaryRef)added, nullptr) != errSecSuccess)
            return {};
        return QString::fromUtf8(generated);
    }
}

} // namespace rc
