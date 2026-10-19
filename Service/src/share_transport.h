#pragma once

#include <rc/transport.h>

#include <QDeadlineTimer>
#include <QHash>
#include <QString>

// 分享端用哪种传输方式（rc/transport.h）。SDK 要求两端一样、一次运行里不变，所以分享端一次运行只能是一种，
// 服务所有来连的人。来连本机的每份信令带着对方这次要的方式（对方先直连，打不通再走中继）：
// - 和现在的一样：照常交给分享会话（Accept）
// - 不一样、本机没人在用：换成对方要的，重开分享会话（Switch）
// - 不一样、有人在用：回绝，对方稍后再试（Busy）。在用：有人连着、在等允许、路断了在回来的路上（ShareHost 知道，
//   inUse），或者别的设备刚来连过、还没连通（这里记着：重开会把它的连接断掉）
// - 要中继、本机没有中继（没连上服务器）：回绝（NoRelay）
// 网页版（浏览器经服务器上的转发进程来连，rc::WebTransport）也是一种，规则一样。
// 用着中继、网页版的转发，没人在用了：换回直连（shouldSwitchBack）
class ShareTransport
{
public:
    enum class Verdict { Accept, Switch, Busy, NoRelay };

    // 一份信令交给分享会话以后多久还算在连：收集地址、连通检查（SDK 默认最多 15 秒）、握手，到等允许那一步
    static constexpr int AttemptMs = 25 * 1000;

    explicit ShareTransport(int attemptMs = AttemptMs);

    QString current() const { return current_; }

    // 设备 from 要用 wanted 连本机。inUse：有人连着、在等允许或在回来的路上；hasRelay：本机有中继
    Verdict check(const QString &from, const QString &wanted, bool inUse, bool hasRelay);
    // 设备 from 的信令交给了分享会话
    void attempting(const QString &from);
    // 设备 from 连通了，到了等允许那一步：之后算在 inUse 里
    void arrived(const QString &from);
    // 换成 transport 重开了分享会话：在连的都断了
    void switchTo(const QString &transport);
    // 用着中继、网页版的转发，没人在用了
    bool shouldSwitchBack(bool inUse);

private:
    // 除了 from 还有别的设备在连
    bool othersAttempting(const QString &from);
    void forgetAttempts();

    const int attemptMs_;
    QString current_{rc::DirectTransport};
    QHash<QString, QDeadlineTimer> attempts_; // 在连的设备，算到什么时候
};
