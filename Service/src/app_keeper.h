#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

// 有人连着本机时界面程序要在：本机的人要看得到谁在连（刘海）、收得到消息、能断开对方。
// - 界面程序意外没了（崩溃、被结束）而还有人连着：把它拉起来（不弹窗口，只在托盘、刘海里）
// - 拉不起来，或者拉起来以后 WaitMs 内没连上后台服务：断开连着的人（本机的人看不到有人在连）
// - 没人在电脑前（装好时，登录界面上没人登录）：不用拉，也不断开（连着的只会是我的设备）
// - 界面程序说它要退出了（本机的人确认过，或者系统注销、关机）：断开连着的人，不拉它
class AppKeeper : public QObject
{
    Q_OBJECT

public:
    static constexpr int WaitMs = 15 * 1000;

    // 启动界面程序的结果
    enum class Launch { Started, Failed, NobodyThere };

    // launch：启动界面程序（不弹窗口）；disconnectAll：断开连着本机的人
    AppKeeper(std::function<Launch()> launch, std::function<void()> disconnectAll, int waitMs = WaitMs,
              QObject *parent = nullptr);

    // 有没有界面程序连着后台服务
    void setAttended(bool attended);
    // 有没有人连着本机
    void setWatched(bool watched);
    // 界面程序要退出了
    void appQuitting();

private:
    void check();
    void gaveUp();

    std::function<Launch()> launch_;
    std::function<void()> disconnectAll_;
    QTimer wait_; // 拉起来以后等它连上
    bool attended_ = false;
    bool watched_ = false;
    bool quitting_ = false; // 界面程序说了要退出，到它下次连上为止
};

// 启动和后台服务放在一起的界面程序（rc/programs.h），不弹窗口（开发版：后台服务和界面程序是同一个用户的）
AppKeeper::Launch launchAppBeside();
