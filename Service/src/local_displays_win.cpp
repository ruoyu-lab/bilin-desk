#include "local_displays.h"

#include <qt_windows.h>

#include <vector>

QHash<quint32, QString> displayNames()
{
    QHash<quint32, QString> names;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG result = ERROR_INSUFFICIENT_BUFFER;
    // 两次调用之间显示器又变了时按新的个数再要一次
    while (result == ERROR_INSUFFICIENT_BUFFER) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (result != ERROR_SUCCESS)
            break;
        paths.resize(pathCount);
        modes.resize(modeCount);
        result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        paths.resize(pathCount);
    }
    if (result != ERROR_SUCCESS) {
        qWarning("读不到显示器的名字（错误 %ld）", result);
        return names;
    }

    // SDK 在 Windows 上的 display_id 是 GDI 设备名 \\.\DISPLAYN 里的 N；名字是显示器报的型号，内置屏常常没有
    constexpr QStringView GdiPrefix = u"\\\\.\\DISPLAY";
    for (const DISPLAYCONFIG_PATH_INFO &path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        DISPLAYCONFIG_TARGET_DEVICE_NAME target = {};
        target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size = sizeof(target);
        target.header.adapterId = path.targetInfo.adapterId;
        target.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS
            || DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS)
            continue;
        const QString device = QString::fromWCharArray(source.viewGdiDeviceName);
        const QString name = QString::fromWCharArray(target.monitorFriendlyDeviceName);
        if (!name.isEmpty())
            names.insert(device.sliced(GdiPrefix.size()).toUInt(), name);
    }
    return names;
}
