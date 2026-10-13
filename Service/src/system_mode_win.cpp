#include "system_mode.h"

#include <rc/log_file.h>
#include <rc/programs.h>
#include <rc/system_service.h>

#include <rcsdk/agent.hpp>
#include <rcsdk/supervisor.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>
#include <wtsapi32.h>

#include <memory>
#include <thread>

namespace system_mode {

namespace {

// 只有 SYSTEM 和管理员能进，里面的跟着
constexpr wchar_t ProtectedDirectory[] = L"O:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
// TrustedInstaller（Program Files 下的东西归它）
constexpr wchar_t TrustedInstaller[] = L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";

QString accountName(PSID sid)
{
    wchar_t name[256]{};
    wchar_t domain[256]{};
    DWORD nameSize = 256;
    DWORD domainSize = 256;
    SID_NAME_USE use{};
    if (LookupAccountSidW(nullptr, sid, name, &nameSize, domain, &domainSize, &use))
        return QString::fromWCharArray(domain) + u'\\' + QString::fromWCharArray(name);
    return QStringLiteral("别的账户");
}

// 能改它的：SYSTEM、管理员，程序（withInstaller）还有 TrustedInstaller
bool trustedWriter(PSID sid, bool withInstaller)
{
    if (IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid))
        return true;
    PSID installer = nullptr;
    if (!withInstaller || !ConvertStringSidToSidW(TrustedInstaller, &installer))
        return false;
    const bool same = EqualSid(sid, installer) != 0;
    LocalFree(installer);
    return same;
}

// path 不只有可信的账户能动时为什么：它的主人、访问列表里让进的每个账户都要可信。onlyWrites：只能读、能运行的
// 不算（程序）
QString protectionProblem(const QString &path, bool onlyWrites)
{
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const std::wstring name = QDir::toNativeSeparators(path).toStdWString();
    const DWORD status = GetNamedSecurityInfoW(name.c_str(), SE_FILE_OBJECT,
                                               OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner,
                                               nullptr, &dacl, nullptr, &descriptor);
    if (status != ERROR_SUCCESS)
        return QStringLiteral("%1 的访问控制读不出来（%2）").arg(path).arg(status);
    const std::unique_ptr<void, decltype(&LocalFree)> owned(descriptor, &LocalFree);
    if (!owner || !trustedWriter(owner, onlyWrites))
        return QStringLiteral("%1 属于%2").arg(path, owner ? accountName(owner) : QStringLiteral("没有人"));
    if (!dacl)
        return QStringLiteral("%1 没有访问控制，谁都能改").arg(path);
    constexpr ACCESS_MASK Writes = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_DELETE_CHILD
                                   | FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE
                                   | GENERIC_ALL | MAXIMUM_ALLOWED;
    for (DWORD i = 0; i < dacl->AceCount; ++i) {
        ACE_HEADER *header = nullptr;
        if (!GetAce(dacl, i, reinterpret_cast<void **>(&header)))
            return QStringLiteral("%1 的访问控制读不出来").arg(path);
        if (header->AceType == ACCESS_DENIED_ACE_TYPE)
            continue;
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE)
            return QStringLiteral("%1 的访问控制里有认不得的项").arg(path);
        // 只给里面新建的东西用的，不管它自己
        if (onlyWrites && (header->AceFlags & INHERIT_ONLY_ACE))
            continue;
        const auto *ace = reinterpret_cast<const ACCESS_ALLOWED_ACE *>(header);
        PSID sid = const_cast<DWORD *>(&ace->SidStart);
        if (!trustedWriter(sid, onlyWrites) && (!onlyWrites || (ace->Mask & Writes)))
            return QStringLiteral("%1 让%2%3").arg(path, accountName(sid), onlyWrites ? QStringLiteral("改它") : QStringLiteral("进"));
    }
    return {};
}

QString appProgram()
{
    return QStandardPaths::findExecutable(rc::program::App, {QCoreApplication::applicationDirPath()});
}

// ---- 系统服务本身 ----

SERVICE_STATUS_HANDLE serviceHandle = nullptr;
HANDLE serviceStop = nullptr;

// 后台服务（看护者启动的那一份）让系统服务启动安装程序：服务的自定义控制码（128~255）。
// 只有 SYSTEM 和管理员发得了（安装时去掉了别的账户的这个权限，setup_win.cpp）
constexpr DWORD InstallUpdateControl = 200;

// 下载、核对过的安装包，静默安装：系统服务的进程不在看护者的作业里，安装程序停掉服务时不会跟着结束
void launchInstaller()
{
    const QString installer = QDir::toNativeSeparators(rc::system_service::updateInstaller());
    if (!QFileInfo::exists(installer)) {
        qWarning("要装新版本，安装包却不在：%s", qUtf8Printable(installer));
        return;
    }
    std::wstring command = (u'"' + installer + QStringLiteral("\" --quiet")).toStdWString();
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        nullptr, &startup, &process)) {
        qWarning("安装程序启动不了（%lu）", GetLastError());
        return;
    }
    qInfo("启动了安装程序：%s", qUtf8Printable(installer));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

void reportStatus(DWORD state, DWORD exitCode = NO_ERROR)
{
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    status.dwWin32ExitCode = exitCode;
    status.dwWaitHint = state == SERVICE_STOP_PENDING ? 15000 : 0;
    SetServiceStatus(serviceHandle, &status);
}

DWORD WINAPI serviceControl(DWORD control, DWORD, LPVOID, LPVOID)
{
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        reportStatus(SERVICE_STOP_PENDING);
        SetEvent(serviceStop);
        return NO_ERROR;
    }
    if (control == InstallUpdateControl) {
        launchInstaller();
        return NO_ERROR;
    }
    return control == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

const char *supervisorEventName(rcs_supervisor_event_type type)
{
    switch (type) {
    case RCS_SUPERVISOR_NO_SESSION: return "没有能运行的会话";
    case RCS_SUPERVISOR_HELPER_STARTED: return "后台服务启动了";
    case RCS_SUPERVISOR_HELPER_EXITED: return "后台服务退出了";
    case RCS_SUPERVISOR_HELPER_FAILED: return "后台服务启动不了";
    case RCS_SUPERVISOR_FAILED: return "看护者停了";
    default: return "?";
    }
}

// 看护者在控制台会话里一直开着一份后台服务，到服务停下为止
DWORD superviseUntilStopped()
{
    const QString directoryProblem = prepareStateDirectory();
    if (!directoryProblem.isEmpty())
        return ERROR_ACCESS_DENIED;
    rc::openLogFile(rc::system_service::stateDirectory(), QStringLiteral("system-service.log"));
    if (const QString problem = programProblem(); !problem.isEmpty()) {
        qCritical("程序没保护好，不以 SYSTEM 运行它：%s", qUtf8Printable(problem));
        return ERROR_ACCESS_DENIED;
    }
    const QString helper = u'"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + QStringLiteral("\" ")
                           + rc::program::HelperArgument;
    try {
        rcsdk::Supervisor supervisor(helper.toStdString(), RCS_HELPER_SYSTEM, [](const rcs_supervisor_event &event) {
            qInfo("看护者：%s（会话 %u，进程 %u）%s", supervisorEventName(event.type), event.os_session,
                  event.process_id, event.message);
        });
        if (const rcs_result result = supervisor.start(); result != RCS_OK) {
            qCritical("看护者起不来：%s", rcsdk::lastCallError().c_str());
            return ERROR_SERVICE_SPECIFIC_ERROR;
        }
        qInfo("系统服务开始运行：%s", qUtf8Printable(helper));
        WaitForSingleObject(serviceStop, INFINITE);
        qInfo("系统服务停下");
        supervisor.stop();
    } catch (const rcsdk::Error &error) {
        qCritical("看护者建不起来：%s", error.what());
        return ERROR_SERVICE_SPECIFIC_ERROR;
    }
    return NO_ERROR;
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    serviceHandle = RegisterServiceCtrlHandlerExW(rc::system_service::WindowsName, serviceControl, nullptr);
    serviceStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    reportStatus(SERVICE_RUNNING);
    const DWORD code = superviseUntilStopped();
    reportStatus(SERVICE_STOPPED, code);
}

} // namespace

QString prepareStateDirectory()
{
    const QString directory = rc::system_service::stateDirectory();
    if (!QFileInfo::exists(directory)) {
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(ProtectedDirectory, SDDL_REVISION_1, &descriptor,
                                                                  nullptr))
            return QStringLiteral("建不了数据目录的访问控制（%1）").arg(GetLastError());
        SECURITY_ATTRIBUTES attributes{sizeof attributes, descriptor, FALSE};
        const std::wstring name = QDir::toNativeSeparators(directory).toStdWString();
        const BOOL created = CreateDirectoryW(name.c_str(), &attributes);
        const DWORD error = GetLastError();
        LocalFree(descriptor);
        if (!created && error != ERROR_ALREADY_EXISTS)
            return QStringLiteral("建不了 %1（%2）").arg(directory).arg(error);
    }
    return protectionProblem(directory, false);
}

QString programProblem()
{
    const QString program = QCoreApplication::applicationFilePath();
    if (const QString problem = protectionProblem(program, true); !problem.isEmpty())
        return problem;
    return protectionProblem(QFileInfo(program).absolutePath(), true);
}

AppKeeper::Launch launchApp()
{
    const DWORD session = WTSGetActiveConsoleSessionId();
    HANDLE token = nullptr;
    if (session == 0xFFFFFFFF || !WTSQueryUserToken(session, &token))
        return GetLastError() == ERROR_NO_TOKEN || session == 0xFFFFFFFF ? AppKeeper::Launch::NobodyThere
                                                                         : AppKeeper::Launch::Failed;
    const std::unique_ptr<void, decltype(&CloseHandle)> user(token, &CloseHandle);
    const QString program = appProgram();
    if (program.isEmpty()) {
        qWarning("后台服务旁边没有界面程序（%s）", rc::program::App.data());
        return AppKeeper::Launch::Failed;
    }
    void *environment = nullptr;
    if (!CreateEnvironmentBlock(&environment, token, FALSE))
        environment = nullptr;
    std::wstring command = (u'"' + QDir::toNativeSeparators(program) + QStringLiteral("\" ") + rc::program::BackgroundArgument)
                               .toStdWString();
    const std::wstring directory = QDir::toNativeSeparators(QFileInfo(program).absolutePath()).toStdWString();
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    wchar_t desktop[] = L"winsta0\\default";
    startup.lpDesktop = desktop;
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessAsUserW(token, nullptr, command.data(), nullptr, nullptr, FALSE,
                                              CREATE_UNICODE_ENVIRONMENT, environment, directory.c_str(), &startup,
                                              &process);
    const DWORD error = GetLastError();
    if (environment)
        DestroyEnvironmentBlock(environment);
    if (!started) {
        qWarning("给控制台前的用户启动界面程序没成（%lu）", error);
        return AppKeeper::Launch::Failed;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return AppKeeper::Launch::Started;
}

void onStopRequest(std::function<void()> stop)
{
    std::thread([stop = std::move(stop)] {
        try {
            if (rcsdk::waitForStopRequest())
                QMetaObject::invokeMethod(QCoreApplication::instance(), stop, Qt::QueuedConnection);
        } catch (const rcsdk::Error &error) {
            // 不是看护者启动的（比如手动运行）：没有谁会要它结束
            qWarning("不是系统服务启动的：%s", error.what());
        }
    }).detach();
}

bool installUpdate()
{
    const SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    const SC_HANDLE service = manager ? OpenServiceW(manager, rc::system_service::WindowsName, SERVICE_USER_DEFINED_CONTROL)
                                      : nullptr;
    SERVICE_STATUS status{};
    const bool sent = service && ControlService(service, InstallUpdateControl, &status);
    if (!sent)
        qWarning("让系统服务启动安装程序没成（%lu）", GetLastError());
    if (service)
        CloseServiceHandle(service);
    if (manager)
        CloseServiceHandle(manager);
    return sent;
}

int runSystemService()
{
    wchar_t name[] = L"";
    SERVICE_TABLE_ENTRYW table[] = {{name, serviceMain}, {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(table) ? 0 : 1;
}

} // namespace system_mode
