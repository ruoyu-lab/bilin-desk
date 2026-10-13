#pragma once

#include <QByteArray>

#include <optional>

namespace rc {

// 存到文件里的秘密（设备令牌、设备私钥）：
// Windows 用 DPAPI 加密，只有这台电脑上的这个用户解得开（文件被备份、拷走了也没用）；
// macOS 原样存：~/Library 只有本人能读。钥匙串要付费的开发者签名，临时签名的程序每次升级以后读钥匙串
// 都会弹窗要钥匙串密码。Android 也原样存：程序自己的数据目录只有本程序能读（user_seal_plain.cpp）
QByteArray sealForUser(const QByteArray &data);
// 解不开（别的用户、别的电脑上加密的，内容坏了）：nullopt
std::optional<QByteArray> unsealForUser(const QByteArray &sealed);

} // namespace rc
