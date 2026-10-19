#pragma once

#include <rc/feedback_logs.h>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class ServiceClient;

// 提交反馈（QML 单例 Feedback，docs/feedback.md）：说了什么、联系方式，默认带上日志。日志先给人看（loadLogs）：
// 界面程序自己的（logFiles）这里读，后台服务的向它要；提交时把看到的这些原样交给后台服务，由它带着设备令牌
// 交给服务器（界面程序拿不到令牌）
class Feedback : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 日志：{ name, size（给人看的大小）}，内容用 logText(index) 取
    Q_PROPERTY(QVariantList logs READ logs NOTIFY logsChanged)
    // 所有日志加起来多大（给人看的）
    Q_PROPERTY(QString logsSize READ logsSize NOTIFY logsChanged)
    // 在等后台服务的日志
    Q_PROPERTY(bool loadingLogs READ loadingLogs NOTIFY logsChanged)
    // 在等后台服务交给服务器
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // 本机的系统（一起交上去的）
    Q_PROPERTY(QString systemName READ systemName CONSTANT)

public:
    // 不给默认构造：QML 才会经 create() 取到 main 创建的这一个
    Feedback(ServiceClient &service, QStringList logFiles, QObject *parent);

    static void setInstance(Feedback *instance);
    static Feedback *create(QQmlEngine *, QJSEngine *);

    QVariantList logs() const;
    QString logsSize() const;
    bool loadingLogs() const { return loadingLogs_; }
    bool busy() const { return busy_; }
    QString systemName() const { return rc::feedback::systemName(); }

    // 重新读一遍日志（打开反馈页时）
    Q_INVOKABLE void loadLogs();
    Q_INVOKABLE QString logText(int index) const;
    // contact：没登录时自己填的（登录着时服务器用账号的邮箱）；withLogs：带不带上 logs
    Q_INVOKABLE void send(const QString &text, const QString &contact, bool withLogs);

signals:
    void logsChanged();
    void busyChanged();
    void sent();
    void failed(const QString &message);

private:
    void setBusy(bool busy);

    ServiceClient &service_;
    const QStringList logFiles_;
    QList<rc::feedback::Log> logs_;
    bool loadingLogs_ = false;
    bool busy_ = false;
};
