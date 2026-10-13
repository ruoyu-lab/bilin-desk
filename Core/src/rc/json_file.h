#pragma once

#include <QJsonObject>
#include <QString>

namespace rc {

// 读一个内容是 JSON 对象的文件。文件不存在时返回空对象；读不了、内容不对时也返回空对象，并记一条警告
QJsonObject readJsonFile(const QString &path);
// 整个换掉（先写到临时文件再替换，写到一半断电也不会坏），失败时记一条警告、返回 false
bool writeJsonFile(const QString &path, const QJsonObject &object);

} // namespace rc
