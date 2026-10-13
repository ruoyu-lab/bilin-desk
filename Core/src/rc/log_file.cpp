#include "rc/log_file.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>

namespace rc {

namespace {

QFile *logFile = nullptr;
QMutex logMutex;
QtMessageHandler previousHandler = nullptr;

char levelLetter(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return 'D';
    case QtInfoMsg: return 'I';
    case QtWarningMsg: return 'W';
    case QtCriticalMsg: return 'E';
    case QtFatalMsg: return 'F';
    }
    return '?';
}

void writeMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        const QMutexLocker locker(&logMutex);
        const QByteArray line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")).toUtf8()
                                + ' ' + levelLetter(type) + ' ' + message.toUtf8() + '\n';
        logFile->write(line);
        logFile->flush();
    }
    previousHandler(type, context, message);
}

} // namespace

void openLogFile(const QString &directory, const QString &name)
{
    Q_ASSERT(!logFile);
    QDir().mkpath(directory);
    const QString path = directory + u'/' + name;
    const QString previous = path + QStringLiteral(".previous");
    QFile::remove(previous);
    QFile::rename(path, previous);

    logFile = new QFile(path);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("日志文件 %s 打不开：%s", qUtf8Printable(path), qUtf8Printable(logFile->errorString()));
        delete logFile;
        logFile = nullptr;
        return;
    }
    previousHandler = qInstallMessageHandler(writeMessage);
}

} // namespace rc
