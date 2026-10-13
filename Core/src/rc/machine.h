#pragma once

#include <QString>

// 这台电脑：重装程序以后登录同一个账号，服务器按它认出是账号下原来的那一台，换回原来的设备 ID（Server/src/hub.ts 的
// signIn）。发给服务器的是硬件编号（macOS 的 IOPlatformUUID、Windows 的 MachineGuid）加上本程序的前缀以后的 sha256，
// 不是编号本身；重装系统以后会变
namespace rc {

// sha256 的十六进制；拿不到硬件编号时为空
QString machineHash();

// 各平台：machine_mac.cpp、machine_win.cpp
QString machineId();

} // namespace rc
