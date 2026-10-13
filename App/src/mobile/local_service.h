#pragma once

#include "control_server.h"
#include "device_store.h"
#include "lan_discovery.h"
#include "lan_signaling.h"
#include "server_link.h"
#include "signaling.h"

#include <QObject>

// 手机上的后台服务：手机上不能另起一个程序在后台跑，所以在本程序的进程里，界面照样经本机连接（ServiceClient，
// rc/protocol.h）和它说话，和电脑上一样。只有去连别人要的那些：本机身份、账号、附近的设备、信令
// （service_wiring.h）。手机不被别人连：来连的一律回绝（peer::NotAccepting）
class LocalService : public QObject
{
    Q_OBJECT

public:
    // directory：本机身份、设置放在哪里（rc/paths.h）
    explicit LocalService(const QString &directory, QObject *parent = nullptr);

    // 在 endpoint 上听界面连上来，开始连服务器、听局域网；听不了时 false
    bool start(const QString &endpoint);

private:
    DeviceStore store_;
    ServerLink link_;
    ControlServer server_;
    LanDiscovery discovery_;
    LanSignaling lan_;
    Signaling signaling_;
};
