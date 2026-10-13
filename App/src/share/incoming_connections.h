#pragma once

#include "incoming_chat.h"
#include "incoming_files.h"
#include "incoming_history.h"

#include <rc/protocol.h>

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Conversations;
class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 别人连本机，给界面用的一面（后台服务的 share 主题）：现在能不能被连、缺什么授权，
// 请求连接本机、等本机的人点允许的，已经连着的（每一项是 { id, deviceId（三位一组）, name, platform, control }）。
// 决定都是给后台服务发命令，它改好以后发回来的新状态才算数
class IncomingConnections : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 别人现在能连本机
    Q_PROPERTY(bool accepting READ accepting NOTIFY changed)
    // 开着"允许别人连接本机"却连不上时为什么：permission（缺屏幕录制）| unavailable（出错了）
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    // macOS 的"屏幕与系统音频录制"和"辅助功能"授权（Windows 上总是有）
    Q_PROPERTY(bool screenPermission READ screenPermission NOTIFY changed)
    Q_PROPERTY(bool inputPermission READ inputPermission NOTIFY changed)
    // 现在能让对方控制本机
    Q_PROPERTY(bool canControl READ canControl NOTIFY changed)
    // 和连着本机的人同步剪贴板（所有人一起）；改是给后台服务发命令
    Q_PROPERTY(bool clipboard READ clipboard WRITE setClipboard NOTIFY changed)
    Q_PROPERTY(QVariantList requests READ requests NOTIFY changed)
    Q_PROPERTY(QVariantList viewers READ viewers NOTIFY changed)
    // 和连着本机的人的对话
    Q_PROPERTY(IncomingChat *chat READ chat CONSTANT)
    // 和连着本机的人互传的文件
    Q_PROPERTY(IncomingFiles *files READ files CONSTANT)
    // 本机的连接记录（通知）
    Q_PROPERTY(IncomingHistory *history READ history CONSTANT)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个
    IncomingConnections(ServiceClient &service, Conversations &conversations, QObject *parent);

    static void setInstance(IncomingConnections *instance);
    static IncomingConnections *create(QQmlEngine *, QJSEngine *);

    bool accepting() const { return state_.accepting; }
    QString problem() const { return state_.problem; }
    bool screenPermission() const { return state_.screenPermission; }
    bool inputPermission() const { return state_.inputPermission; }
    bool canControl() const { return state_.canControl; }
    bool clipboard() const { return state_.clipboard; }
    void setClipboard(bool clipboard);
    QVariantList requests() const;
    QVariantList viewers() const;
    IncomingChat *chat() { return &chat_; }
    IncomingFiles *files() const { return files_; }
    IncomingHistory *history() const { return history_; }

    // control：能不能控制本机，否则只能看
    Q_INVOKABLE void admit(int id, bool control);
    Q_INVOKABLE void refuse(int id);
    Q_INVOKABLE void disconnectViewer(int id);
    Q_INVOKABLE void setControl(int id, bool control);
    // 去授权（permission：screen | input）：后台服务弹出系统的授权提示（只有第一次会弹），同时打开系统设置里的那一页
    Q_INVOKABLE void requestPermission(const QString &permission);
    // 出了错、别人连不上本机时再试一次
    Q_INVOKABLE void retry();

signals:
    void changed();

private:
    ServiceClient &service_;
    rc::protocol::ShareState state_;
    IncomingChat chat_;
    IncomingFiles *files_;
    IncomingHistory *history_;
};
