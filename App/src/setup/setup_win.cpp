#include "setup_platform.h"

#include <rc/paths.h>
#include <rc/programs.h>
#include <rc/system_service.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUuid>

#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace setup_platform {

namespace {

// 应用和功能里的登记（64 位的视图）
constexpr wchar_t UninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Bilin";
// 界面程序在当前用户下记的：偏好设置（QSettings）、登录时运行（login_item_win.cpp）
constexpr wchar_t SettingsKey[] = L"Software\\Bilin";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunValue[] = L"Bilin";
// 防火墙里两条规则（界面程序、后台服务）用同一个名字，卸载时按名字一起删
constexpr wchar_t FirewallRule[] = L"比邻";
// 让服务能发 Ctrl+Alt+Del（SDK 的 RCS_INPUT_SECURE_ATTENTION 要它）：1 是服务，3 是服务和辅助功能程序。
// 装之前是什么记在登记里（PreviousSas；没有这个值时是 NoSas），卸载时改回去
constexpr wchar_t PolicyKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
constexpr wchar_t SasValue[] = L"SoftwareSASGeneration";
constexpr wchar_t PreviousSasValue[] = L"PreviousSas";
constexpr DWORD NoSas = 0xFFFFFFFF;
// 程序目录只有 SYSTEM 和管理员能改，用户能读、能运行：系统服务以 SYSTEM 运行里面的程序
constexpr wchar_t ProgramAccess[] = L"O:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;0x1200a9;;;BU)";
// 系统服务的权限：和系统默认的一样，只是别的账户（交互登录的 IU、服务 SU）没有自定义控制码（CR）——
// 后台服务用它让系统服务启动安装程序（system_mode_win.cpp），只该 SYSTEM 和管理员发
constexpr wchar_t ServiceAccess[] = L"D:(A;;CCLCSWRPWPDTLOCRRC;;;SY)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)"
                                    L"(A;;CCLCSWLORC;;;IU)(A;;CCLCSWLORC;;;SU)";

QString displayName()
{
    return QCoreApplication::translate("Setup", "比邻");
}

QString knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &path)))
        return {};
    const QString result = QString::fromWCharArray(path);
    CoTaskMemFree(path);
    return result;
}

QString programIn(const QString &directory, QLatin1StringView name)
{
    return QDir::toNativeSeparators(QDir(directory).filePath(QString(name) + QStringLiteral(".exe")));
}

QString shortcutPath()
{
    return QDir(knownFolder(FOLDERID_CommonPrograms)).filePath(displayName() + QStringLiteral(".lnk"));
}

// 所有用户的桌面
QString desktopShortcutPath()
{
    return QDir(knownFolder(FOLDERID_PublicDesktop)).filePath(displayName() + QStringLiteral(".lnk"));
}

bool samePath(const QString &a, const QString &b)
{
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
}


bool isElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof elevation, &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

// 命令行的一个参数：有空格就加引号（路径末尾不会是反斜杠）
QString quoted(const QString &argument)
{
    return argument.contains(u' ') ? u'"' + argument + u'"' : argument;
}

// 用管理员身份启动（弹 UAC）；用户没允许时 false
bool startElevated(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    for (const QString &argument : arguments)
        parts.append(quoted(argument));
    const std::wstring file = program.toStdWString();
    const std::wstring parameters = parts.join(u' ').toStdWString();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof info;
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = file.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&info))
        return true;
    qWarning("用管理员身份启动不了（%lu）：%s", GetLastError(), qUtf8Printable(program));
    return false;
}

QString readString(HKEY key, const wchar_t *name)
{
    DWORD bytes = 0;
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
        return {};
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(value.c_str());
}

bool writeString(HKEY key, const wchar_t *name, const QString &value)
{
    const std::wstring text = value.toStdWString();
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(text.c_str()),
                          DWORD((text.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool writeNumber(HKEY key, const wchar_t *name, DWORD value)
{
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&value), sizeof value) == ERROR_SUCCESS;
}

// 停掉 directory 下面正在运行的程序（升级、卸载前：界面程序、后台服务；不包括自己）
QString stopProgramsIn(const QString &directory)
{
    const QString prefix = QDir::toNativeSeparators(QDir::cleanPath(directory)) + u'\\';
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return QCoreApplication::translate("Setup", "看不了正在运行的程序（错误 %1）").arg(GetLastError());
    QString error;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof entry;
    for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
        if (entry.th32ProcessID == GetCurrentProcessId())
            continue;
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                           entry.th32ProcessID);
        if (!process)
            continue;
        wchar_t path[MAX_PATH * 4];
        DWORD length = MAX_PATH * 4;
        if (QueryFullProcessImageNameW(process, 0, path, &length)
            && QString::fromWCharArray(path, int(length)).startsWith(prefix, Qt::CaseInsensitive)) {
            qInfo("安装：停掉 %s", qUtf8Printable(QString::fromWCharArray(path, int(length))));
            if (!TerminateProcess(process, 0) || WaitForSingleObject(process, 10000) != WAIT_OBJECT_0)
                error = QCoreApplication::translate("Setup", "停不掉正在运行的比邻，关掉它再试");
        }
        CloseHandle(process);
    }
    CloseHandle(snapshot);
    return error;
}

// 换目录的名字：刚停掉的程序、杀毒软件扫描新文件时，目录里的文件可能还被占着一会儿，隔一会儿再试，最多 10 秒
bool renameDirectory(const QString &from, const QString &to)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (QDir().rename(from, to))
            return true;
        Sleep(200);
    }
    return false;
}

// 把 from 下面的文件都复制到 to（to 是新建的空目录）
QString copyTree(const QString &from, const QString &to, const std::function<void(double)> &progress, qint64 *size)
{
    struct File
    {
        QString relative;
        qint64 size;
    };
    QList<File> files;
    qint64 total = 0;
    const QDir source(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        files.append({source.relativeFilePath(info.filePath()), info.size()});
        total += info.size();
    }
    qint64 copied = 0;
    const QDir target(to);
    for (const File &file : std::as_const(files)) {
        const QString destination = target.filePath(file.relative);
        if (!QDir().mkpath(QFileInfo(destination).path()) || !QFile::copy(source.filePath(file.relative), destination))
            return QCoreApplication::translate("Setup", "复制不了 %1：磁盘满了，或者没有权限").arg(file.relative);
        copied += file.size;
        progress(total > 0 ? double(copied) / double(total) : 1);
    }
    *size = total;
    return {};
}

QString removeTree(const QString &directory)
{
    if (!QFileInfo::exists(directory) || QDir(directory).removeRecursively())
        return {};
    return QCoreApplication::translate("Setup", "删不掉 %1：里面有文件正在用").arg(QDir::toNativeSeparators(directory));
}

// 防火墙：netsh 的命令行原样给（参数里的引号要它自己认）
QString netsh(const QString &arguments)
{
    QProcess process;
    process.setProgram(QStringLiteral("netsh.exe"));
    process.setNativeArguments(arguments);
    process.start();
    if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return QCoreApplication::translate("Setup", "改不了防火墙的设置");
    return {};
}

void removeFirewallRules()
{
    // 没有这个名字的规则时 netsh 也报错：卸载、撤回时不管
    netsh(QStringLiteral("advfirewall firewall delete rule name=\"%1\"").arg(QString::fromWCharArray(FirewallRule)));
}

QString addFirewallRules(const QString &target)
{
    removeFirewallRules();
    for (const QLatin1StringView name : {rc::program::App, rc::program::Service}) {
        const QString error = netsh(QStringLiteral("advfirewall firewall add rule name=\"%1\" dir=in action=allow "
                                                   "program=\"%2\" enable=yes profile=any")
                                        .arg(QString::fromWCharArray(FirewallRule), programIn(target, name)));
        if (!error.isEmpty())
            return error;
    }
    return {};
}

QString createShortcut(const QString &link, const QString &program)
{
    // 后台线程上：这个线程自己的 COM
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW *shell = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell));
    if (SUCCEEDED(result)) {
        shell->SetPath(program.toStdWString().c_str());
        shell->SetWorkingDirectory(QFileInfo(program).path().toStdWString().c_str());
        shell->SetDescription(displayName().toStdWString().c_str());
        IPersistFile *file = nullptr;
        result = shell->QueryInterface(IID_PPV_ARGS(&file));
        if (SUCCEEDED(result)) {
            result = file->Save(QDir::toNativeSeparators(link).toStdWString().c_str(), TRUE);
            file->Release();
        }
        shell->Release();
    }
    if (SUCCEEDED(initialized))
        CoUninitialize();
    if (FAILED(result))
        return QCoreApplication::translate("Setup", "建不了开始菜单里的快捷方式");
    return {};
}

// 应用和功能里登记过的版本（升级没成时改回去）
struct Registration
{
    bool existed = false;
    QString version;
    DWORD sizeKb = 0;
};

QString registerUninstall(const QString &target, qint64 bytes, Registration *previous)
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UninstallKey, 0, nullptr, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &key,
                        &disposition)
        != ERROR_SUCCESS)
        return QCoreApplication::translate("Setup", "登记不到应用和功能里");
    previous->existed = disposition == REG_OPENED_EXISTING_KEY;
    previous->version = readString(key, L"DisplayVersion");
    DWORD size = sizeof previous->sizeKb;
    RegGetValueW(key, nullptr, L"EstimatedSize", RRF_RT_REG_DWORD, nullptr, &previous->sizeKb, &size);
    const QString program = programIn(target, rc::program::App);
    const bool ok = writeString(key, L"DisplayName", displayName())
                    && writeString(key, L"DisplayVersion", QCoreApplication::applicationVersion())
                    && writeString(key, L"Publisher", displayName())
                    && writeString(key, L"DisplayIcon", program)
                    && writeString(key, L"InstallLocation", QDir::toNativeSeparators(target))
                    && writeString(key, L"UninstallString", quoted(program) + QStringLiteral(" --setup uninstall"))
                    && writeNumber(key, L"NoModify", 1) && writeNumber(key, L"NoRepair", 1)
                    && writeNumber(key, L"EstimatedSize", DWORD(bytes / 1024));
    RegCloseKey(key);
    if (!ok)
        return QCoreApplication::translate("Setup", "登记不到应用和功能里");
    return {};
}

void unregisterUninstall()
{
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, UninstallKey, KEY_WOW64_64KEY, 0);
}

void restoreRegistration(const Registration &previous)
{
    if (!previous.existed) {
        unregisterUninstall();
        return;
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UninstallKey, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return;
    writeString(key, L"DisplayVersion", previous.version);
    writeNumber(key, L"EstimatedSize", previous.sizeKb);
    RegCloseKey(key);
}

// ---- 系统服务（rc/system_service.h）----

struct ServiceHandle
{
    void operator()(SC_HANDLE handle) const { CloseServiceHandle(handle); }
};
using Service = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ServiceHandle>;

Service openService(DWORD access)
{
    const Service manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    return Service(manager ? OpenServiceW(manager.get(), rc::system_service::WindowsName, access) : nullptr);
}

// 停下来（等它停完，最多 30 秒）；没装、本来就停着的不管
// wasRunning：停之前它在运行（装没成时撤回要再启动它）
QString stopService(bool *wasRunning = nullptr)
{
    const Service service = openService(SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!service)
        return {};
    SERVICE_STATUS status{};
    if (wasRunning)
        *wasRunning = QueryServiceStatus(service.get(), &status) && status.dwCurrentState != SERVICE_STOPPED;
    if (!ControlService(service.get(), SERVICE_CONTROL_STOP, &status) && GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
        return QCoreApplication::translate("Setup", "停不下后台服务（错误 %1）").arg(GetLastError());
    for (int i = 0; i < 300 && QueryServiceStatus(service.get(), &status) && status.dwCurrentState != SERVICE_STOPPED; ++i)
        Sleep(100);
    if (status.dwCurrentState != SERVICE_STOPPED)
        return QCoreApplication::translate("Setup", "后台服务停不下来");
    return {};
}

// 装好服务（升级时换成新的命令行）并启动：开机就运行，出了错 5 秒后再启动。created：这一次新建的
QString installService(const QString &target, bool *created)
{
    const Service manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (!manager)
        return QCoreApplication::translate("Setup", "装不了后台服务（错误 %1）").arg(GetLastError());
    const std::wstring command = (u'"' + programIn(target, rc::program::Service) + QStringLiteral("\" ")
                                  + rc::program::SystemServiceArgument)
                                     .toStdWString();
    const std::wstring name = displayName().toStdWString();
    Service service(OpenServiceW(manager.get(), rc::system_service::WindowsName, SERVICE_ALL_ACCESS));
    *created = !service;
    if (service) {
        if (!ChangeServiceConfigW(service.get(), SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                  command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, name.c_str()))
            return QCoreApplication::translate("Setup", "改不了后台服务（错误 %1）").arg(GetLastError());
    } else {
        service.reset(CreateServiceW(manager.get(), rc::system_service::WindowsName, name.c_str(), SERVICE_ALL_ACCESS,
                                     SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                     command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
        if (!service)
            return QCoreApplication::translate("Setup", "装不了后台服务（错误 %1）").arg(GetLastError());
    }
    PSECURITY_DESCRIPTOR access = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(ServiceAccess, SDDL_REVISION_1, &access, nullptr))
        return QCoreApplication::translate("Setup", "设不了后台服务的权限（错误 %1）").arg(GetLastError());
    const BOOL secured = SetServiceObjectSecurity(service.get(), DACL_SECURITY_INFORMATION, access);
    LocalFree(access);
    if (!secured)
        return QCoreApplication::translate("Setup", "设不了后台服务的权限（错误 %1）").arg(GetLastError());
    std::wstring description = QCoreApplication::translate("Setup", "让你的其他设备随时能连这台电脑，锁屏、没登录时也行")
                                   .toStdWString();
    SERVICE_DESCRIPTIONW text{description.data()};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &text);
    SC_ACTION restart[3] = {{SC_ACTION_RESTART, 5000}, {SC_ACTION_RESTART, 5000}, {SC_ACTION_RESTART, 30000}};
    SERVICE_FAILURE_ACTIONSW failure{86400, nullptr, nullptr, 3, restart};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS, &failure);
    if (!StartServiceW(service.get(), 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
        return QCoreApplication::translate("Setup", "后台服务启动不了（错误 %1）").arg(GetLastError());
    return {};
}

// 装没成、撤回时：把停掉的服务再启动起来
void startService()
{
    if (const Service service = openService(SERVICE_START); service && !StartServiceW(service.get(), 0, nullptr))
        qWarning("安装：原来的后台服务启动不了（错误 %lu）", GetLastError());
}

void removeService()
{
    stopService();
    if (const Service service = openService(DELETE))
        DeleteService(service.get());
}

// 程序目录只有 SYSTEM 和管理员能改（里面的跟着）
QString protectProgram(const QString &target)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(ProgramAccess, SDDL_REVISION_1, &descriptor, nullptr))
        return QCoreApplication::translate("Setup", "保护不了程序目录（错误 %1）").arg(GetLastError());
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    PSID owner = nullptr;
    GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted);
    GetSecurityDescriptorOwner(descriptor, &owner, &defaulted);
    std::wstring path = QDir::toNativeSeparators(target).toStdWString();
    const DWORD result = SetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT,
                                               OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION
                                                   | PROTECTED_DACL_SECURITY_INFORMATION,
                                               owner, nullptr, dacl, nullptr);
    LocalFree(descriptor);
    if (result != ERROR_SUCCESS)
        return QCoreApplication::translate("Setup", "保护不了程序目录（错误 %1）").arg(result);
    return {};
}

std::optional<DWORD> readNumber(HKEY root, const wchar_t *path, const wchar_t *name)
{
    DWORD value = 0;
    DWORD size = sizeof value;
    if (RegGetValueW(root, path, name, RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY, nullptr, &value, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return value;
}

// 让服务能发 Ctrl+Alt+Del；装之前的值记在登记里（升级时已经记过，不再记）
QString allowSecureAttention()
{
    HKEY policy = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, PolicyKey, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                        nullptr, &policy, nullptr)
        != ERROR_SUCCESS)
        return QCoreApplication::translate("Setup", "改不了 Ctrl+Alt+Del 的策略");
    const std::optional<DWORD> current = readNumber(HKEY_LOCAL_MACHINE, PolicyKey, SasValue);
    if (!readNumber(HKEY_LOCAL_MACHINE, UninstallKey, PreviousSasValue)) {
        HKEY uninstall = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UninstallKey, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &uninstall) == ERROR_SUCCESS) {
            writeNumber(uninstall, PreviousSasValue, current.value_or(NoSas));
            RegCloseKey(uninstall);
        }
    }
    const bool ok = (current && (*current == 1 || *current == 3)) || writeNumber(policy, SasValue, 1);
    RegCloseKey(policy);
    return ok ? QString() : QCoreApplication::translate("Setup", "改不了 Ctrl+Alt+Del 的策略");
}

// 改回装之前的
void restoreSecureAttention()
{
    const std::optional<DWORD> previous = readNumber(HKEY_LOCAL_MACHINE, UninstallKey, PreviousSasValue);
    HKEY policy = nullptr;
    if (!previous || RegOpenKeyExW(HKEY_LOCAL_MACHINE, PolicyKey, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &policy) != ERROR_SUCCESS)
        return;
    if (*previous == NoSas)
        RegDeleteValueW(policy, SasValue);
    else
        writeNumber(policy, SasValue, *previous);
    RegCloseKey(policy);
}

// 当前用户下界面程序记的：偏好设置、登录时运行
void removeUserSettings()
{
    RegDeleteTreeW(HKEY_CURRENT_USER, SettingsKey);
    HKEY run = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RunKey, 0, KEY_SET_VALUE, &run) == ERROR_SUCCESS) {
        RegDeleteValueW(run, RunValue);
        RegCloseKey(run);
    }
}

} // namespace

QString installedPath()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UninstallKey, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return {};
    const QString path = readString(key, L"InstallLocation");
    RegCloseKey(key);
    return path;
}

bool runningInstalled()
{
    const QString path = installedPath();
    return !path.isEmpty() && samePath(QCoreApplication::applicationDirPath(), path);
}

QString defaultInstallPath()
{
    return QDir::toNativeSeparators(QDir(knownFolder(FOLDERID_ProgramFiles)).filePath(QStringLiteral("Bilin")));
}

bool relaunchElevated()
{
    if (isElevated())
        return false;
    // 用户没允许就不装（没有管理员权限的账户装不了：装服务、防火墙都要管理员）
    startElevated(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    return true;
}

bool relaunchForUninstall(const Request &request)
{
    const QString here = QCoreApplication::applicationDirPath();
    if (!runningInstalled()) {
        // 已经是临时目录里的那一份：还要管理员身份
        if (isElevated())
            return false;
        startElevated(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
        return true;
    }
    // 从装好的那一份启动的：复制到临时目录，从那里用管理员身份再启动（才删得掉程序目录）
    const QString copy = QDir::temp().filePath(QStringLiteral("BilinUninstall-")
                                               + QUuid::createUuid().toString(QUuid::Id128).left(8));
    qint64 size = 0;
    const QString error = copyTree(here, copy, [](double) {}, &size);
    if (!error.isEmpty()) {
        qWarning("卸载：%s", qUtf8Printable(error));
        return true;
    }
    QStringList arguments{QStringLiteral("--setup"), QStringLiteral("uninstall"), QStringLiteral("--target"),
                          QDir::toNativeSeparators(here), QStringLiteral("--data"),
                          QDir::toNativeSeparators(rc::stateDirectory())};
    if (request.quiet)
        arguments.append(QStringLiteral("--quiet"));
    startElevated(programIn(copy, rc::program::App), arguments);
    return true;
}

bool hasDesktopShortcut()
{
    return QFileInfo::exists(desktopShortcutPath());
}

std::vector<SetupStep> installSteps(const QString &source, const QString &target, bool desktopShortcut)
{
    const QString fresh = target + QStringLiteral(".new");
    const QString old = target + QStringLiteral(".old");
    auto bytes = std::make_shared<qint64>(0);
    auto registration = std::make_shared<Registration>();
    auto shortcutExisted = std::make_shared<bool>(false);
    auto desktopExisted = std::make_shared<bool>(false);
    auto serviceCreated = std::make_shared<bool>(false);
    auto serviceWasRunning = std::make_shared<bool>(false);
    return {
        // 先停系统服务：不然看护者把停掉的后台服务又启动起来。没装成时原来的那一份换回去以后再启动它
        {QStringLiteral("停掉正在运行的比邻"), 1,
         [target, serviceWasRunning](const auto &) {
             const QString error = stopService(serviceWasRunning.get());
             return error.isEmpty() ? stopProgramsIn(target) : error;
         },
         [serviceWasRunning] {
             if (*serviceWasRunning)
                 startService();
         }},
        {QStringLiteral("复制程序"), 20,
         [source, fresh, bytes](const auto &progress) {
             const QString error = removeTree(fresh);
             return error.isEmpty() ? copyTree(source, fresh, progress, bytes.get()) : error;
         },
         [fresh] { removeTree(fresh); }},
        // 先放在旁边复制好再换上：换不了时原来那一份不受影响
        {QStringLiteral("换上新的"), 1,
         [target, fresh, old](const auto &) {
             if (QFileInfo::exists(target)) {
                 if (const QString error = removeTree(old); !error.isEmpty())
                     return error;
                 if (!renameDirectory(target, old))
                     return QCoreApplication::translate("Setup", "换不了原来装的那一份：里面有文件正在用");
             }
             if (!renameDirectory(fresh, target)) {
                 renameDirectory(old, target);
                 return QCoreApplication::translate("Setup", "装不到 %1").arg(QDir::toNativeSeparators(target));
             }
             return QString();
         },
         [target, fresh, old] {
             renameDirectory(target, fresh);
             if (QFileInfo::exists(old))
                 renameDirectory(old, target);
         }},
        {QStringLiteral("保护程序目录"), 1, [target](const auto &) { return protectProgram(target); }, {}},
        {QStringLiteral("登记到应用和功能"), 1,
         [target, bytes, registration](const auto &) { return registerUninstall(target, *bytes, registration.get()); },
         [registration] { restoreRegistration(*registration); }},
        {QStringLiteral("开始菜单里的快捷方式"), 1,
         [target, shortcutExisted](const auto &) {
             *shortcutExisted = QFileInfo::exists(shortcutPath());
             return createShortcut(shortcutPath(), programIn(target, rc::program::App));
         },
         [shortcutExisted] {
             if (!*shortcutExisted)
                 QFile::remove(shortcutPath());
         }},
        // 选了就放（升级时也换成新的位置），没选就去掉；撤回时回到装之前的样子
        {QStringLiteral("桌面上的快捷方式"), 1,
         [target, desktopShortcut, desktopExisted](const auto &) {
             *desktopExisted = hasDesktopShortcut();
             if (desktopShortcut)
                 return createShortcut(desktopShortcutPath(), programIn(target, rc::program::App));
             QFile::remove(desktopShortcutPath());
             return QString();
         },
         [target, desktopExisted] {
             if (*desktopExisted)
                 createShortcut(desktopShortcutPath(), programIn(target, rc::program::App));
             else
                 QFile::remove(desktopShortcutPath());
         }},
        {QStringLiteral("防火墙放行"), 1, [target](const auto &) { return addFirewallRules(target); },
         [] { removeFirewallRules(); }},
        {QStringLiteral("允许发 Ctrl+Alt+Del"), 1, [](const auto &) { return allowSecureAttention(); },
         [] { restoreSecureAttention(); }},
        // 最后一步：启动以后它核对程序目录保护好了没有
        {QStringLiteral("装好并启动后台服务"), 2,
         [target, serviceCreated](const auto &) { return installService(target, serviceCreated.get()); },
         [serviceCreated] {
             if (*serviceCreated)
                 removeService();
         }},
        {QStringLiteral("清理"), 1, [old](const auto &) { return removeTree(old); }, {}},
    };
}

std::vector<SetupStep> uninstallSteps(const Request &request)
{
    const QString target = request.target;
    const QString data = request.data;
    return {
        {QStringLiteral("去掉后台服务"), 2, [](const auto &) {
             removeService();
             return QString();
         }, {}},
        {QStringLiteral("停掉正在运行的比邻"), 1, [target](const auto &) { return stopProgramsIn(target); }, {}},
        // 登记里记着装之前的，要在去掉登记以前
        {QStringLiteral("改回 Ctrl+Alt+Del 的策略"), 1, [](const auto &) {
             restoreSecureAttention();
             return QString();
         }, {}},
        {QStringLiteral("去掉防火墙的放行"), 1, [](const auto &) {
             removeFirewallRules();
             return QString();
         }, {}},
        {QStringLiteral("去掉开始菜单、桌面上的快捷方式"), 1, [](const auto &) {
             QFile::remove(shortcutPath());
             QFile::remove(desktopShortcutPath());
             return QString();
         }, {}},
        {QStringLiteral("从应用和功能里去掉"), 1, [](const auto &) {
             unregisterUninstall();
             return QString();
         }, {}},
        {QStringLiteral("删除程序"), 6, [target](const auto &) { return removeTree(target); }, {}},
        // 用户的（开发版用过的）和整台电脑那一份（系统服务的身份、设置、日志）
        {QStringLiteral("删除数据"), 1, [data](const auto &) {
             removeUserSettings();
             if (const QString error = data.isEmpty() ? QString() : removeTree(data); !error.isEmpty())
                 return error;
             return removeTree(rc::system_service::stateDirectory());
         }, {}},
    };
}

void openInstalled(const QString &target)
{
    // 经资源管理器启动：不带安装程序的管理员身份
    QProcess::startDetached(QStringLiteral("explorer.exe"), {programIn(target, rc::program::App)});
}

void cleanUpAfterExit()
{
    // 卸载用的临时那一份：等这个进程退出以后再删（自己正在用着删不掉）
    const QString here = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    if (runningInstalled() || !here.startsWith(QDir::toNativeSeparators(QDir::tempPath()), Qt::CaseInsensitive))
        return;
    QProcess process;
    process.setProgram(QStringLiteral("cmd.exe"));
    process.setNativeArguments(QStringLiteral("/c ping -n 4 127.0.0.1 >nul & rmdir /s /q \"%1\"").arg(here));
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
    process.startDetached();
}

} // namespace setup_platform
