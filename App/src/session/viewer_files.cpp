#include "viewer_files.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#include "mobile/mobile_files.h"

#include <QThread>

#include <memory>
#endif

ViewerFiles::ViewerFiles(rc::InterruptedTransfers &interrupted, const QString &peer, QObject *parent)
    : FileTransferList(parent)
    , files_(interrupted)
{
    connect(&files_, &rc::FileTransfers::changed, this, [this] { setTransfers(files_.transfers()); });
    if (!peer.isEmpty())
        files_.setPeer(0, peer);
}

QString ViewerFiles::send(const QList<QUrl> &urls)
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    auto copied = std::make_shared<QStringList>();
    QThread *copying = QThread::create([urls, copied] {
        for (const QUrl &url : urls) {
            const QString path = mobile_files::copyForSending(url);
            if (path.isEmpty())
                return;
            copied->append(path);
        }
    });
    connect(copying, &QThread::finished, copying, &QObject::deleteLater);
    connect(copying, &QThread::finished, this, [this, copied, count = urls.size()] {
        if (copied->size() == count)
            files_.send(0, *copied);
        else
            emit sendFailed(tr("有的文件读不了"));
    });
    copying->start();
    return {};
#else
    QStringList paths;
    if (const QString problem = localPaths(urls, paths); !problem.isEmpty())
        return problem;
    files_.send(0, paths);
    return {};
#endif
}

void ViewerFiles::clearCompleted()
{
    files_.clearCompleted();
}

void ViewerFiles::reject(int id)
{
    files_.reject(id);
}

void ViewerFiles::cancel(int id)
{
    files_.cancel(id);
}

void ViewerFiles::retry(int id)
{
    files_.retry(id);
}

void ViewerFiles::remove(int id)
{
    files_.remove(id);
}

void ViewerFiles::acceptInto(int id, const QString &directory)
{
    files_.accept(id, directory);
}
