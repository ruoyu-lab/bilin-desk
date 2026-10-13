#include "rc/device_name.h"

#include <QSysInfo>

namespace rc {

QString defaultDeviceName()
{
    return QSysInfo::machineHostName();
}

} // namespace rc
