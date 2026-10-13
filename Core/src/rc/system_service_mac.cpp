#include "rc/system_service.h"

#include <QFileInfo>

namespace rc::system_service {

namespace {

// 后台服务（root）建的，放套接字
constexpr QLatin1StringView RunDirectory{"/var/run/com.bilindesk.bilin"};

} // namespace

bool installed()
{
    return QFileInfo::exists(daemonPlist());
}

QString endpoint()
{
    return RunDirectory + QStringLiteral("/service");
}

QString stateDirectory()
{
    return QStringLiteral("/Library/Application Support/Bilin/Service");
}

QString updateInstaller()
{
    return stateDirectory() + QStringLiteral("/updates/Bilin-update.pkg");
}

QString daemonPlist()
{
    return QStringLiteral("/Library/LaunchDaemons/%1.plist").arg(QLatin1StringView(DaemonLabel));
}

QString agentPlist()
{
    return QStringLiteral("/Library/LaunchAgents/%1.plist").arg(QLatin1StringView(AgentLabel));
}

QString agentEndpoint(QLatin1StringView access)
{
    return RunDirectory + QStringLiteral("/agent-") + access;
}

QString agentKey()
{
    return QStringLiteral("/Library/Application Support/Bilin/agent.key");
}

} // namespace rc::system_service
