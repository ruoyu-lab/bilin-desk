#include "rc/system_service.h"

#include <QDir>

#include <windows.h>

#include <shlobj.h>

namespace rc::system_service {

bool installed()
{
    const SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager)
        return false;
    const SC_HANDLE service = OpenServiceW(manager, WindowsName, SERVICE_QUERY_STATUS);
    if (service)
        CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return service != nullptr;
}

QString endpoint()
{
    return QStringLiteral("com.bilindesk.bilin.system");
}

QString stateDirectory()
{
    PWSTR data = nullptr;
    QString directory;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &data)))
        directory = QDir::fromNativeSeparators(QString::fromWCharArray(data)) + QStringLiteral("/Bilin");
    CoTaskMemFree(data);
    return directory;
}

QString updateInstaller()
{
    return stateDirectory() + QStringLiteral("/updates/Bilin-update.exe");
}

} // namespace rc::system_service
