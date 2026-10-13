#include "agent_gate.h"

#include <rc/programs.h>

#include <QCoreApplication>
#include <QDir>
#include <QMutex>

#include <SystemConfiguration/SystemConfiguration.h>

#include <libproc.h>

namespace {

// 控制台前的用户的名字；登录窗口上（没人登录）是 root
QString consoleUser()
{
    uid_t uid = 0;
    CFStringRef name = SCDynamicStoreCopyConsoleUser(nullptr, &uid, nullptr);
    if (!name)
        return QStringLiteral("root");
    const QString user = QString::fromCFString(name);
    CFRelease(name);
    return user == u"loginwindow" ? QStringLiteral("root") : user;
}

QString processPath(uint32_t pid)
{
    char path[PROC_PIDPATHINFO_MAXSIZE]{};
    return proc_pidpath(int(pid), path, sizeof path) > 0 ? QString::fromUtf8(path) : QString();
}

class ConsoleAgentGate final : public rcsdk::AgentGate
{
public:
    explicit ConsoleAgentGate(std::function<void(const rcs_agent_info &)> seen)
        : program_(QDir(QCoreApplication::applicationDirPath()).filePath(rc::program::Agent))
        , seen_(std::move(seen))
    {
    }

    bool attach(const rcs_agent_info &agent) override
    {
        seen_(agent);
        const QString user = consoleUser();
        QString problem;
        if (const QString path = processPath(agent.process_id); path != program_)
            problem = QStringLiteral("进程 %1 是 %2，不是本程序的助手").arg(agent.process_id).arg(path);
        else if (QString::fromUtf8(agent.account) != user)
            problem = QStringLiteral("它以 %1 运行，控制台前是 %2").arg(QString::fromUtf8(agent.account), user);
        else if (agent.screen_permission != RCS_PERMISSION_GRANTED)
            problem = QStringLiteral("%1 的助手还没有屏幕录制授权").arg(user);
        // 同一个原因只记一次（助手每隔几秒再来）
        const QMutexLocker lock(&mutex_);
        if (problem.isEmpty()) {
            last_.clear();
            return true;
        }
        if (problem != last_)
            qInfo("不认这个助手：%s", qUtf8Printable(problem));
        last_ = problem;
        return false;
    }

private:
    const QString program_;
    const std::function<void(const rcs_agent_info &)> seen_;
    QMutex mutex_;
    QString last_;
};

} // namespace

std::unique_ptr<rcsdk::AgentGate> consoleAgentGate(std::function<void(const rcs_agent_info &)> seen)
{
    return std::make_unique<ConsoleAgentGate>(std::move(seen));
}
