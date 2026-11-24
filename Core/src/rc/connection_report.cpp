#include "rc/connection_report.h"

#include <QJsonArray>

namespace rc {

namespace {

QString routeName(ConnectionReport::Route route)
{
    switch (route) {
    case ConnectionReport::Route::Direct: return QStringLiteral("direct");
    case ConnectionReport::Route::Relay: return QStringLiteral("relay");
    case ConnectionReport::Route::RelayTls: return QStringLiteral("relayTls");
    }
    return {};
}

QString pathName(NetworkPath path)
{
    switch (path) {
    case NetworkPath::Lan: return QStringLiteral("lan");
    case NetworkPath::Direct: return QStringLiteral("direct");
    case NetworkPath::Relay: return QStringLiteral("relay");
    case NetworkPath::Unknown: return {};
    }
    return {};
}

// 服务器收的上限（Server/src/protocol.ts）：原因 200 个字、试过的路 8 段
constexpr qsizetype MaxReason = 200;
constexpr qsizetype MaxRoutes = 8;

} // namespace

void ConnectionReport::begin(const QString &target, qint64 nowMs)
{
    *this = {};
    active_ = true;
    target_ = target;
    beganMs_ = nowMs;
}

void ConnectionReport::attempt(Route route, const QString &access)
{
    access_ = access;
    const QString name = routeName(route);
    // 换临时密码再连、对方让再连一次：路没变，不重复记
    if ((routes_.isEmpty() || routes_.constLast() != name) && routes_.size() < MaxRoutes)
        routes_.append(name);
}

void ConnectionReport::connected(NetworkPath path, qint64 nowMs)
{
    if (connectedMs_ < 0)
        connectedMs_ = nowMs;
    if (path != NetworkPath::Unknown)
        path_ = path;
}

void ConnectionReport::sample(double fps, double mbps, double latencyMs, const QString &codec, const QString &resolution)
{
    ++samples_;
    fps_ += fps;
    mbps_ += mbps;
    latencyMs_ += latencyMs;
    if (!codec.isEmpty())
        codec_ = codec;
    if (!resolution.isEmpty())
        resolution_ = resolution;
}

QJsonObject ConnectionReport::finish(const QString &reason, qint64 nowMs)
{
    const bool connected = connectedMs_ >= 0;
    const auto average = [this](double total) { return samples_ > 0 ? total / samples_ : 0.0; };
    const QJsonObject report{
        {QStringLiteral("target"), target_},
        {QStringLiteral("access"), access_},
        {QStringLiteral("result"), connected            ? QStringLiteral("connected")
                                   : reason.isEmpty()   ? QStringLiteral("cancelled")
                                                        : QStringLiteral("failed")},
        {QStringLiteral("reason"), reason.left(MaxReason)},
        {QStringLiteral("routes"), QJsonArray::fromStringList(routes_)},
        {QStringLiteral("path"), connected ? pathName(path_) : QString()},
        {QStringLiteral("connectMs"), connected ? connectedMs_ - beganMs_ : -1},
        {QStringLiteral("durationS"), connected ? (nowMs - connectedMs_) / 1000 : 0},
        {QStringLiteral("reconnects"), reconnects_},
        {QStringLiteral("approval"), approval_},
        {QStringLiteral("control"), control_},
        {QStringLiteral("codec"), codec_},
        {QStringLiteral("resolution"), resolution_},
        {QStringLiteral("fps"), average(fps_)},
        {QStringLiteral("mbps"), average(mbps_)},
        {QStringLiteral("latencyMs"), average(latencyMs_)},
    };
    *this = {};
    return report;
}

} // namespace rc
