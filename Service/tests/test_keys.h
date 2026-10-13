#pragma once

#include <QByteArray>
#include <QRandomGenerator>

// 测试用的设备密钥：样子对（97 字节，0x04 开头），不是真的 P-256 密钥对
inline QByteArray testDeviceKey()
{
    QByteArray key(97, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32 *>(key.data() + 1), 24);
    key[0] = 0x04;
    return key;
}
