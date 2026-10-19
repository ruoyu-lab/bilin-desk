#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>

#include <optional>

// 反馈（docs/feedback.md）带的日志。界面程序读它自己的（app.log、macOS 的 agent.log），后台服务读它的
// （service.log……，装好时只有 SYSTEM / root 读得了），给人看过以后由后台服务交给服务器。
// 界面和后台服务之间一条消息最多 1 MB（rc/line_channel.h），日志压缩了再传（packLogs）
namespace rc::feedback {

// 一份日志最多带末尾这么多：服务器那边所有日志加起来最多 6 MB（Server/src/feedback.ts）
inline constexpr qsizetype MaxLogBytes = 512 * 1024;

struct Log
{
    QString name; // 文件名，如 service.log
    QString text;

    friend bool operator==(const Log &, const Log &) = default;
};

// path 末尾最多 MaxLogBytes，从截下的那一段里第一个整行开始；没有这个文件、是空的时 nullopt
std::optional<Log> readLog(const QString &path);
// 这些文件里有的（readLog），按给的顺序
QList<Log> readLogs(const QStringList &paths);

// [{"name", "data"}]：data 是 UTF-8 文本经 qCompress 压缩再 base64
QJsonArray packLogs(const QList<Log> &logs);
// 不对的（不是这个格式、解不开、解开以后超过 MaxLogBytes）：nullopt
std::optional<QList<Log>> unpackLogs(const QJsonArray &packed);

// 本机的系统，给人看的，如"macOS 15.3（arm64）"
QString systemName();

} // namespace rc::feedback
