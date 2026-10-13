#pragma once

#include <QString>
#include <QStringList>

// 本机连接上来的界面程序（ControlServer 的客户端，socket 是它的本机连接）是谁，能做什么。
// 装好时后台服务以 SYSTEM / root 运行（rc/system_service.h），每个用户的界面程序都连它：
// - 只让控制台前的用户（坐在这台电脑前、看得到刘海的人）和管理员连上来（admit）
// - Windows：互传文件由后台服务自己读写（SDK 的文件传输模块在它的进程里），替界面程序发文件、收下文件以前
//   核对那个用户自己能不能读、能不能写，不然普通用户能借它读写任何文件。macOS 上读写文件的是用户自己的助手
namespace client_rights {

bool admit(qintptr socket);
// paths（文件、文件夹，文件夹里的每一项都算）都能读
bool mayRead(qintptr socket, const QStringList &paths);
// 能在 directory 里建文件
bool mayWrite(qintptr socket, const QString &directory);

} // namespace client_rights
