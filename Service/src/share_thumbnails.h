#pragma once

#include <rc/control_protocol.h>

#include <rcsdk/rcsdk.h>

#include <QList>
#include <QObject>
#include <QSet>

#include <functional>
#include <thread>

namespace rcsdk {
class SessionConfig;
}

// 观看端换显示器之前先看一眼：它要每一块的缩略图（rc/control_protocol.h 的 thumbnails），SDK 的缩略图模块
// 在采集的那台机器上截（装好的 macOS、Windows 上在助手那里），截好的经 send 发回给它。
// 截一块要等回答（最多 5 秒），在另一个线程上一块一块截。同一时间只截一轮：截着时又有人要，截完再截一轮给他们。
// 截图用着分享会话：会话停之前 stop()
class ShareThumbnails : public QObject
{
    Q_OBJECT

public:
    // send：在本对象的线程上发给观看端 viewer
    explicit ShareThumbnails(std::function<void(int viewer, const QByteArray &message)> send,
                             QObject *parent = nullptr);
    ~ShareThumbnails() override;

    static void configure(rcsdk::SessionConfig &config);
    // 观看端 viewer 要 displays（SDK 的 display_id）这几块的缩略图；session：正在运行的分享会话
    void request(int viewer, rcs_session session, const QList<quint32> &displays);
    // 分享会话要停了：等截着的这一轮截完，截好的、排着的都不要了
    void stop();

private:
    void start(rcs_session session, const QList<quint32> &displays);
    void finished(quint64 round, const QList<rc::control::DisplayPicture> &pictures);

    std::function<void(int, const QByteArray &)> send_;
    std::thread worker_;
    quint64 round_ = 0;
    QSet<int> taking_;  // 这一轮截给谁
    QSet<int> waiting_; // 截着时又要的：下一轮
    rcs_session session_ = nullptr;
    QList<quint32> displays_;
};
