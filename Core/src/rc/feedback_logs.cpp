#include "rc/feedback_logs.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSysInfo>
#include <QtEndian>

namespace rc::feedback {

std::optional<Log> readLog(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const qint64 size = file.size();
    const bool cut = size > MaxLogBytes;
    if (cut)
        file.seek(size - MaxLogBytes);
    QByteArray bytes = file.readAll();
    // 从中间截的：前面半行不要
    if (cut)
        bytes.remove(0, bytes.indexOf('\n') + 1);
    if (bytes.isEmpty())
        return std::nullopt;
    return Log{QFileInfo(path).fileName(), QString::fromUtf8(bytes)};
}

QList<Log> readLogs(const QStringList &paths)
{
    QList<Log> logs;
    for (const QString &path : paths) {
        if (std::optional<Log> log = readLog(path))
            logs.append(std::move(*log));
    }
    return logs;
}

QJsonArray packLogs(const QList<Log> &logs)
{
    QJsonArray packed;
    for (const Log &log : logs) {
        packed.append(QJsonObject{{QStringLiteral("name"), log.name},
                                  {QStringLiteral("data"), QString::fromLatin1(qCompress(log.text.toUtf8()).toBase64())}});
    }
    return packed;
}

std::optional<QList<Log>> unpackLogs(const QJsonArray &packed)
{
    QList<Log> logs;
    for (const QJsonValue &value : packed) {
        const QJsonObject item = value.toObject();
        const QString name = item.value(QLatin1StringView("name")).toString();
        const QByteArray data = QByteArray::fromBase64(item.value(QLatin1StringView("data")).toString().toLatin1());
        // qCompress 在前面写了解开以后多大（4 字节，大端）：先看它，免得照着一个假的大小去分配
        if (name.isEmpty() || data.size() < 4 || qFromBigEndian<quint32>(data.constData()) > MaxLogBytes)
            return std::nullopt;
        const QByteArray text = qUncompress(data);
        if (text.isEmpty())
            return std::nullopt;
        logs.append(Log{name, QString::fromUtf8(text)});
    }
    return logs;
}

QString systemName()
{
    return QStringLiteral("%1（%2）").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
}

} // namespace rc::feedback
