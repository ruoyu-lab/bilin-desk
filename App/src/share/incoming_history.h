#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class ServiceClient;

// 本机的连接记录（IncomingConnections 的 history，主窗口工具栏的通知）：后台服务记着（history 主题），
// 谁什么时候连过本机、请求没成的、输错密码的、互传的文件。看过到哪一条记在偏好设置里，后来的算没看过
class IncomingHistory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("是 IncomingConnections 的 history")

    // 新的在前，每一项是 rc::protocol::HistoryEntry 的样子（deviceId 三位一组，at、endedAt 是 Date，
    // endedAt 没断开时为 null）
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    // 没看过的几条
    Q_PROPERTY(int unread READ unread NOTIFY changed)

public:
    IncomingHistory(ServiceClient &service, QObject *parent);

    QVariantList entries() const { return entries_; }
    int unread() const;

    // 都看过了
    Q_INVOKABLE void markSeen();
    // 清空（后台服务里的也清掉）
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void update();

    ServiceClient &service_;
    QVariantList entries_;
    int newest_ = 0; // 最新一条的编号
    int seen_ = 0;   // 看过到哪一条
};
