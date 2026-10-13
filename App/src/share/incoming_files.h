#pragma once

#include "files/file_transfer_list.h"

class ServiceClient;

// 别人连本机时互传的文件（IncomingConnections 的 files）：被控提示的卡片里。传输在后台服务里（files 主题），
// 这里是它的样子和给它的命令；收下时存到哪里由这边给（设置里选的文件夹）
class IncomingFiles : public FileTransferList
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("是 IncomingConnections 的 files")

public:
    IncomingFiles(ServiceClient &service, QObject *parent);

    // 选的文件和文件夹发给连着本机的人 viewer（IncomingConnections.viewers 的 id）。
    // 返回空字符串表示发了，否则是给人看的原因
    Q_INVOKABLE QString send(int viewer, const QList<QUrl> &urls);

    void reject(int id) override;
    void cancel(int id) override;
    void retry(int id) override;
    void remove(int id) override;

protected:
    void acceptInto(int id, const QString &directory) override;

private:
    ServiceClient &service_;
};
