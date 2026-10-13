#include "login_item.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <windows.h>

#include <string>
#include <vector>

namespace login_item {

namespace {

// 当前用户登录时运行的程序
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
// 用户在任务管理器、设置的"启动"里开关的记在这里，值和 Run 里的同名：第一个字节是奇数表示关掉了
constexpr wchar_t ApprovedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t ValueName[] = L"Bilin";
// 登录时运行的命令带着它：只在托盘，不弹窗口
constexpr QStringView AtLoginArgument = u"--autostart";

// 登录时运行的命令：完整路径加引号（路径里可能有空格），后面是 AtLoginArgument
QString command()
{
    return QStringLiteral("\"%1\" %2").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
                                           AtLoginArgument);
}

// Run 里的那一项是这一份程序（路径不分大小写）。别处的一份（另一个构建、安装目录）登记的不算
bool registered()
{
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, RunKey, ValueName, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
        return false;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, RunKey, ValueName, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS)
        return false;
    return QString::fromWCharArray(value.c_str()).compare(command(), Qt::CaseInsensitive) == 0;
}

// 用户在任务管理器或设置的"启动"里关掉了
bool disabledByUser()
{
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, ApprovedKey, ValueName, RRF_RT_REG_BINARY, nullptr, nullptr, &bytes) != ERROR_SUCCESS
        || bytes == 0)
        return false;
    std::vector<BYTE> data(bytes);
    if (RegGetValueW(HKEY_CURRENT_USER, ApprovedKey, ValueName, RRF_RT_REG_BINARY, nullptr, data.data(), &bytes)
        != ERROR_SUCCESS)
        return false;
    return (data[0] & 1) != 0;
}

} // namespace

Status status()
{
    if (!registered())
        return Status::Off;
    return disabledByUser() ? Status::NeedsApproval : Status::On;
}

void setEnabled(bool enabled)
{
    LSTATUS result;
    if (enabled) {
        const std::wstring value = command().toStdWString();
        result = RegSetKeyValueW(HKEY_CURRENT_USER, RunKey, ValueName, REG_SZ, value.c_str(),
                                 DWORD((value.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteKeyValueW(HKEY_CURRENT_USER, RunKey, ValueName);
    }
    if (result != ERROR_SUCCESS)
        qWarning("改不了登录时运行的程序（%ld）", long(result));
}

void openSystemSettings()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:startupapps")));
}

void whenLaunched(std::function<void(bool atLogin)> handler)
{
    // 和 macOS 一样等事件循环开始：那时界面已经加载好了
    QTimer::singleShot(0, [handler = std::move(handler)] {
        handler(QCoreApplication::arguments().contains(AtLoginArgument));
    });
}

} // namespace login_item
