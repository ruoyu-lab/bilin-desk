#pragma once

#include <QString>

namespace rc::ipc {

// 本机连接（QLocalServer / QLocalSocket）用的名字，每个用户一个：同一台电脑上别的账户有自己的
// （换了数据目录的，每个目录一个，rc/paths.h 的 StateDirectoryVariable）。
// base 如 "com.bilindesk.bilin.service"
QString userScopedName(const QString &base);

// 后台服务的本机连接
QString serviceEndpoint();

} // namespace rc::ipc
