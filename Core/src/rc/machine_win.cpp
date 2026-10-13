#include "rc/machine.h"

#include <windows.h>

namespace rc {

// 装系统时生成，64 位的视图里
QString machineId()
{
    wchar_t guid[64]{};
    DWORD size = sizeof guid;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid",
                     RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, guid, &size) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(guid);
}

} // namespace rc
