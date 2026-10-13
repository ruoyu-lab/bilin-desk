#pragma once

#include <rc/protocol.h>
#include <rc/transfer_list.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

// 本机的连接记录（界面上的通知）：谁什么时候连过本机、控制还是只看、请求没成的、输错临时密码的、互传的文件，
// 还有程序自己更新了。
// 存在状态目录的 history.json，只留最近 MaxEntries 条，新的在前。后台服务上次没好好退出时还"连着"的，
// 当作在文件最后一次写下的时候断开
class ConnectionHistory : public QObject
{
    Q_OBJECT

public:
    static constexpr qsizetype MaxEntries = 200;

    // now：现在的时间（UTC 毫秒），测试里换成假的
    explicit ConnectionHistory(QString file, std::function<qint64()> now = {}, QObject *parent = nullptr);

    rc::protocol::HistoryState state() const { return {entries_}; }

    // 设备连进来了、走了；session 认出同一次连接
    void connected(const rc::protocol::SharePeer &viewer, const QString &session);
    void gone(const QString &device, const QString &session);
    // 请求连接没成（reason 见 rc::protocol::HistoryEntry）
    void refused(const rc::protocol::SharePeer &request, const QString &reason);
    void wrongPin();
    // 和连着本机的人 peer 互传完了的
    void transferred(const rc::protocol::SharePeer &peer, const rc::Transfer &transfer);
    // 程序自己更新到了 version（Updater）
    void updated(const QString &version);
    // 我的设备 peer 让本机 action（restart | shutDown | sleep | signOut），本机答应了
    void power(const rc::protocol::SharePeer &peer, const QString &action);
    void clear();

signals:
    void changed();

private:
    void add(rc::protocol::HistoryEntry entry);
    void load();
    void store();

    const QString file_;
    const std::function<qint64()> now_;
    QList<rc::protocol::HistoryEntry> entries_;
    QHash<QString, int> open_; // 还连着的：设备 ID + session → 记录的 id
    int nextId_ = 1;
};
