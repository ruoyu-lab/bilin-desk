#include "rc/json_file.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace rc {

QJsonObject readJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("读不了 %s：%s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning("%s 的内容不对：%s", qUtf8Printable(path), qUtf8Printable(error.errorString()));
        return {};
    }
    return document.object();
}

bool writeJsonFile(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        if (file.commit())
            return true;
    }
    qWarning("写不了 %s：%s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
    return false;
}

} // namespace rc
