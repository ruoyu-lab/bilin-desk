#include "viewer_files.h"

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
    QStringList paths;
    if (const QString problem = localPaths(urls, paths); !problem.isEmpty())
        return problem;
    files_.send(0, paths);
    return {};
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
