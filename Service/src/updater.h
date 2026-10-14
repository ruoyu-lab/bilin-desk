#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>

#include <rc/protocol.h>

#include <functional>
#include <optional>

// 新版本从哪里来：服务器上的 /updates/<平台>/latest 和它说的安装包（Server/src/server.ts）
class UpdateSource
{
public:
    virtual ~UpdateSource() = default;
    // 现在发布的版本：问到了 ok（没发布过时 release 为空）；问不了服务器时 ok 为 false
    virtual void latest(const QString &platform, std::function<void(bool ok, std::optional<QJsonObject> release)> done) = 0;
    // 把 path（服务器上的地址）下载到 file，超过 maxBytes 就不要了；progress 是已经收到多少，成了 done(true)
    virtual void download(const QString &path, const QString &file, qint64 maxBytes, std::function<void(qint64)> progress,
                          std::function<void(bool)> done) = 0;
};

// 经 HTTP(S) 从服务器取。base 是服务器的地址（https://www.bilindesk.com），证书按系统信任的证书机构校验
class HttpUpdateSource : public UpdateSource
{
public:
    explicit HttpUpdateSource(QUrl base);

    // 服务器长连接的地址（wss://主机/ws）换成下载用的（https://主机）
    static QUrl baseFor(const QUrl &serverUrl);

    void latest(const QString &platform, std::function<void(bool ok, std::optional<QJsonObject> release)> done) override;
    void download(const QString &path, const QString &file, qint64 maxBytes, std::function<void(qint64)> progress,
                  std::function<void(bool)> done) override;

private:
    class QNetworkReply *get(const QString &path);

    const QUrl base_;
    QNetworkAccessManager network_;
};

// 装好的程序的新版本（docs/admin.md）：隔一阵问服务器有没有新版本，有了就告诉界面（update 主题），本机的人点了"更新"
// 才下载、核对发布密钥的签名、大小、sha256，再启动安装（装的时候后台服务、界面程序会被停掉、换掉）。
// 装完以后新的后台服务启动：记下装好了（justUpdated），界面程序重新打开；装了却还是旧的版本，告诉界面没装上
class Updater : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString installer; // 下载的安装包放在这里（rc::system_service::updateInstaller），旁边是记录 update.json；
                           // 那个目录只有 SYSTEM / root 能进（手机上是程序自己的目录）
        QString platform;  // rc::release::platform()
        QVersionNumber version;
        QByteArray publicKey; // rc::release::publicKey()
        int firstCheckMs = 2 * 60 * 1000;
        int checkMs = 6 * 3600 * 1000;
    };

    // install：启动 installer 那个安装程序，启动了时 true
    Updater(Options options, UpdateSource &source, std::function<bool()> install, QObject *parent = nullptr);

    void start();
    rc::protocol::UpdateState state() const { return state_; }
    // 界面上点了"更新"
    void installNow();
    // 设置里点了"检查更新"
    void checkNow();
    // 安装是交给系统的安装界面的（Android）：回到程序时还在装，就是那边取消了、没装上
    void installAbandoned();
    // 这次启动是装完新版本以后的第一次
    bool justUpdated() const { return justUpdated_; }

signals:
    void stateChanged();

private:
    // 服务器上发布的、签名核对过的新版本
    struct Release
    {
        QString version;
        qint64 size = 0;
        QByteArray sha256;
        QString path;
    };

    // asked：是本机的人点了检查更新，结果告诉界面
    void check(bool asked);
    void received(const QJsonObject &release);
    void downloaded(bool ok);
    void setState(rc::protocol::UpdateState state);
    QString directory() const;
    QString recordFile() const;

    const Options options_;
    UpdateSource &source_;
    const std::function<bool()> install_;
    QTimer timer_;
    bool checking_ = false;
    bool justUpdated_ = false;
    std::optional<Release> release_;
    rc::protocol::UpdateState state_;
};
