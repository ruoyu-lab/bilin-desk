#pragma once

#include <rc/control_protocol.h>

#include <QHash>
#include <QList>
#include <QString>

namespace rcsdk {
class Session;
}

// 本机的显示器（分享会话采集的那台机器）：SDK 列出的，加上系统给的名字，按桌面上的位置从左到右、从上到下排
QList<rc::control::Display> localDisplays(const rcsdk::Session &session);

// 系统给显示器的名字（显示器报的型号，或者"内建视网膜显示器"这样的），按 SDK 的 display_id；
// 系统不说名字的不在里面。各平台一份：local_displays_mac.mm、local_displays_win.cpp
QHash<quint32, QString> displayNames();
