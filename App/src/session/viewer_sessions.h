#pragma once

#include "viewer_session.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class Account;
class Conversations;
class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 去连别人的所有会话，按会话窗口给的 key（设备列表里的那一项）各一个。会话属于这里而不是某个窗口：
// 标签在窗口之间移动时连接不断。后台服务转来的信令按连接编号交给对应的会话。
// 会话里的对话就是和 key 那台设备的对话（Conversations）
class ViewerSessions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个。
    // decodeDevice：Windows 上 SDK 解码用的设备（ViewGraphics::decodeDevice）
    // account：去连我的设备时不要密码（ViewerSession）；transfers：去连别人时没传完的文件记在哪里（每个会话共用）
    ViewerSessions(ServiceClient &service, Account &account, void *decodeDevice, Conversations &conversations,
                   rc::InterruptedTransfers &transfers, QObject *parent);

    static void setInstance(ViewerSessions *instance);
    static ViewerSessions *create(QQmlEngine *, QJSEngine *);

    // key 的会话，没有就新建一个（等用户输入密码）。target 是要连的设备 ID
    Q_INVOKABLE ViewerSession *open(const QString &key, const QString &target);
    Q_INVOKABLE ViewerSession *get(const QString &key) const;
    // 断开并忘掉
    Q_INVOKABLE void close(const QString &key);

private:
    void serviceEvent(const QString &name, const QJsonObject &arguments);
    ViewerSession *byConnection(const QString &connection) const;

    ServiceClient &service_;
    Account &account_;
    void *decodeDevice_;
    Conversations &conversations_;
    rc::InterruptedTransfers &transfers_;
    QHash<QString, ViewerSession *> sessions_;
};
