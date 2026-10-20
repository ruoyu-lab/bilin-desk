#pragma once

class ControlServer;
class FeedbackSender;
class LanDiscovery;
class LanSignaling;
class ServerLink;
class Signaling;

// 后台服务里和界面、和"去连别人"有关的连线。电脑上的后台服务（Service/main.cpp）和手机程序进程里的那一份
// （App/src/mobile/local_service.h）共用；被别人连（分享会话）的那一半只在电脑上，main.cpp 自己连
namespace service_wiring {

// 账号：发验证码、注册、登录、找回密码、退出、移出设备、改昵称、改密码、注销经服务器；结果（账号和账号下的设备）给界面。
// 叫醒我的设备也经服务器（请账号下开着的别的电脑帮忙），结果给界面
void connectAccount(ControlServer &server, ServerLink &link);

// P2P 和中继用的服务器（ice 主题，服务器发来的）、附近的设备（nearby 主题）给界面
void publishNetwork(ControlServer &server, ServerLink &link, LanDiscovery &discovery);

// 信令：界面去连别人的会话 ↔ 服务器或局域网（谁的交给谁、走哪条路见 Signaling）
void connectViewerSignaling(ControlServer &server, ServerLink &link, LanSignaling &lan, Signaling &signaling);

// 反馈：界面要后台服务的日志（给人看）、提交反馈，回答只给发命令的那个界面
void connectFeedback(ControlServer &server, FeedbackSender &feedback);

} // namespace service_wiring
