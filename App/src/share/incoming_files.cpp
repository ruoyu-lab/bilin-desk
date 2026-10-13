#include "incoming_files.h"

#include "service_client.h"

#include <rc/protocol.h>

#include <QJsonArray>

namespace protocol = rc::protocol;

IncomingFiles::IncomingFiles(ServiceClient &service, QObject *parent)
    : FileTransferList(parent)
    , service_(service)
{
    connect(&service_, &ServiceClient::topicChanged, this, [this](const QString &topic) {
        if (topic == protocol::FilesTopic)
            setTransfers(rc::transfersFromJson(service_.topic(protocol::FilesTopic)));
    });
}

QString IncomingFiles::send(int viewer, const QList<QUrl> &urls)
{
    QStringList paths;
    if (const QString problem = localPaths(urls, paths); !problem.isEmpty())
        return problem;
    service_.send(protocol::SendFiles,
                  {{QStringLiteral("viewer"), viewer}, {QStringLiteral("paths"), QJsonArray::fromStringList(paths)}});
    return {};
}

void IncomingFiles::reject(int id)
{
    service_.send(protocol::RejectTransfer, {{QStringLiteral("id"), id}});
}

void IncomingFiles::cancel(int id)
{
    service_.send(protocol::CancelTransfer, {{QStringLiteral("id"), id}});
}

void IncomingFiles::retry(int id)
{
    service_.send(protocol::RetryTransfer, {{QStringLiteral("id"), id}});
}

void IncomingFiles::remove(int id)
{
    service_.send(protocol::RemoveTransfer, {{QStringLiteral("id"), id}});
}

void IncomingFiles::acceptInto(int id, const QString &directory)
{
    service_.send(protocol::AcceptTransfer, {{QStringLiteral("id"), id}, {QStringLiteral("directory"), directory}});
}
