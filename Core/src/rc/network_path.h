#pragma once

#include <QStringView>

namespace rc {

// 两台机器之间的连接走的是哪种路，给人看。SDK 只给一段说明（rcs_network_stats.path）：
//   P2P 本机地址 <-> 对方地址 (对方的候选类型)       host 是对方网卡上的地址，srflx、prflx 是 NAT 外面看到的
//   relay 中继地址 on TURN 服务器 ... <-> 对方地址 (…) 经本机的 TURN 服务器
// 对方的候选类型是 relay 时是对方在用中继
enum class NetworkPath {
    Unknown, // 还没连上，或者不是这两种
    Lan,     // 直连，对方在局域网里（私有、链路本地地址）
    Direct,  // 直连，跨网络（打洞，或者对方有公网地址）
    Relay,   // 经中继服务器
};

NetworkPath networkPath(QStringView path);

} // namespace rc
