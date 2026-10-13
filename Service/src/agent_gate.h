#pragma once

#include <rcsdk/agent.hpp>

#include <functional>
#include <memory>

// macOS 装好时，分享会话认哪个助手（SDK 的 agent link，rc/system_service.h）：和后台服务装在一起的助手程序
// （rc/programs.h 的 Agent），以控制台前的用户运行（登录窗口上是 root），有屏幕录制授权。别的拒绝，它过一会儿再试，
// 控制台前换了人、授了权以后就认了。
// seen：每个来连的助手（认没认）都交给它，授权以它为准；在 SDK 的线程上，要很快返回
std::unique_ptr<rcsdk::AgentGate> consoleAgentGate(std::function<void(const rcs_agent_info &)> seen);
