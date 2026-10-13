#pragma once

// 程序收到的退出（QEvent::Quit）是不是用户自己要退：macOS 的 Cmd+Q、程序菜单和 Dock 里的"退出"是；
// 注销、重新启动、关机不是（这时不能拦，拦了系统会说本程序挡住了注销）。
// Windows 上用户只从托盘退出（那里先问过了），收到的退出都是系统的
bool quitAskedByUser();
