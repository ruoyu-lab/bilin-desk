#pragma once

#include <rcsdk/power.h>

#include <QLatin1StringView>
#include <QStringList>
#include <QStringView>

#include <array>
#include <optional>
#include <utility>

// 我的设备之间远程重启、关机、睡眠、注销（SDK 的电源模块）：操作的名字（界面、连接记录里用）和 SDK 的电源操作对上。
// 只给链接了 SDK 电源模块的程序用（界面程序、后台服务），rccore 本身不链接 SDK
namespace rc::power {

inline constexpr QLatin1StringView Restart{"restart"};
inline constexpr QLatin1StringView ShutDown{"shutDown"};
inline constexpr QLatin1StringView Sleep{"sleep"};
inline constexpr QLatin1StringView SignOut{"signOut"};

inline constexpr std::array<std::pair<QLatin1StringView, rcs_power_action>, 4> Actions{{
    {Restart, RCS_POWER_RESTART},
    {ShutDown, RCS_POWER_SHUT_DOWN},
    {Sleep, RCS_POWER_SLEEP},
    {SignOut, RCS_POWER_SIGN_OUT},
}};

inline std::optional<rcs_power_action> action(QStringView name)
{
    for (const auto &[n, a] : Actions) {
        if (n == name)
            return a;
    }
    return std::nullopt;
}

inline QLatin1StringView name(rcs_power_action action)
{
    for (const auto &[n, a] : Actions) {
        if (a == action)
            return n;
    }
    return {};
}

// (1u << rcs_power_action) 的位 → 名字，按 Actions 的顺序
inline QStringList names(uint32_t bits)
{
    QStringList list;
    for (const auto &[n, a] : Actions) {
        if (bits & (1u << a))
            list.append(n);
    }
    return list;
}

} // namespace rc::power
