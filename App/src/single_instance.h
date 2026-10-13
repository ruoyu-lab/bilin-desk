#pragma once

#include <QLocalServer>
#include <QObject>

// 每个用户只运行一个程序。第一个启动的在本机的本地套接字（Windows 是命名管道）上监听；
// 后启动的连上去，说一声"把窗口拿出来"，然后自己退出
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // 已经有一个在运行：让它把窗口拿出来，返回 true（调用者退出）。否则开始监听，返回 false
    bool forwardToRunning();

signals:
    // 又有人启动了程序：把窗口拿出来
    void activated();

private:
    QString name_;
    QLocalServer server_;
};
