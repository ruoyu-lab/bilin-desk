#pragma once

#include "rc/network_path.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace rc {

// 一次去连别的设备的经过（连接统计，docs/connection-stats.md）：界面里去连别人的会话（观看端）边连边记，
// 连上以后断开、没连上、还没连上就被关掉时整理成一条（finish），经后台服务交给服务器（protocol.h 的 ReportConnection，
// 用户在设置里关了就不发）。格式和服务器那一边一样（Server/src/protocol.ts 的 ConnectionReport）。
// 只记怎么连的、连没连上、连了多久、画面大概怎么样：不记画面、输入、对话、文件，也不记地址。
// 一次从开始去连，到连上以后断开或者放弃为止：中间直连不通改走中继、换临时密码再连都算这一次。
// 时间由调用的一方给（毫秒，单调的时钟），测试里能随便给
class ConnectionReport
{
public:
    // 依次试的路：直连、经 UDP 的中继、经中继的 TLS
    enum class Route { Direct, Relay, RelayTls };

    bool active() const { return active_; }
    // 这一次连上过
    bool connected() const { return connectedMs_ >= 0; }
    // 开始去连。target：设备 ID；按局域网的 IP 地址连时是空的（地址不报）
    void begin(const QString &target, qint64 nowMs);
    // 这一次试的路、怎么证明自己（rc/transport.h 的 DeviceAccess、PasswordAccess）：每试一次说一次
    void attempt(Route route, const QString &access);
    // 等对方的人点允许
    void waiting() { approval_ = true; }
    // 连上了（路断了又回来也说）；path：现在走的路
    void connected(NetworkPath path, qint64 nowMs);
    void reconnecting() { ++reconnects_; }
    // 对方让本机控制了
    void controlled() { control_ = true; }
    // 连着时每秒一次：每秒帧数、码率（Mbit/s）、往返延迟（毫秒），编码格式、分辨率（给人看的）
    void sample(double fps, double mbps, double latencyMs, const QString &codec, const QString &resolution);
    // 结束。reason：程序告诉用户的原因；空的是用户自己断开、关掉的。返回这一条，之后又是没在连
    QJsonObject finish(const QString &reason, qint64 nowMs);

private:
    bool active_ = false;
    QString target_;
    QString access_;
    QStringList routes_;
    qint64 beganMs_ = 0;
    qint64 connectedMs_ = -1; // 第一次连上的时候；没连上过是 -1
    NetworkPath path_ = NetworkPath::Unknown;
    int reconnects_ = 0;
    bool approval_ = false;
    bool control_ = false;
    QString codec_;
    QString resolution_;
    int samples_ = 0;
    double fps_ = 0;
    double mbps_ = 0;
    double latencyMs_ = 0;
};

} // namespace rc
