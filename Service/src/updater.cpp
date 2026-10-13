#include "updater.h"

#include <rc/json_file.h>
#include <rc/release.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>

#include <memory>

namespace {

constexpr QLatin1StringView UpdatesPath{"/updates/"};

QByteArray fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex();
}

} // namespace

// ---- HttpUpdateSource ----

HttpUpdateSource::HttpUpdateSource(QUrl base)
    : base_(std::move(base))
{
}

QUrl HttpUpdateSource::baseFor(const QUrl &serverUrl)
{
    QUrl base = serverUrl;
    base.setScheme(serverUrl.scheme() == u"wss" ? QStringLiteral("https") : QStringLiteral("http"));
    base.setPath({});
    return base;
}

QNetworkReply *HttpUpdateSource::get(const QString &path)
{
    QUrl url = base_;
    url.setPath(path);
    return network_.get(QNetworkRequest(url));
}

void HttpUpdateSource::latest(const QString &platform, std::function<void(bool, std::optional<QJsonObject>)> done)
{
    QNetworkReply *reply = get(UpdatesPath + platform + QStringLiteral("/latest"));
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 404：还没发布过这个平台的
        if (status == 404) {
            done(true, std::nullopt);
            return;
        }
        const QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
        if (reply->error() != QNetworkReply::NoError || !json.isObject()) {
            qWarning("问新版本没成：%s", qUtf8Printable(reply->errorString()));
            done(false, std::nullopt);
            return;
        }
        done(true, json.object());
    });
}

void HttpUpdateSource::download(const QString &path, const QString &file, qint64 maxBytes,
                                std::function<void(qint64)> progress, std::function<void(bool)> done)
{
    auto output = std::make_shared<QFile>(file);
    if (!output->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("存不了下载的安装包 %s", qUtf8Printable(file));
        done(false);
        return;
    }
    QNetworkReply *reply = get(path);
    // 收到的写进文件；比说好的大（不是那个安装包）、写不进去时不要了
    const auto append = [reply, output, maxBytes, progress = std::move(progress)] {
        const QByteArray chunk = reply->readAll();
        if (output->size() + chunk.size() > maxBytes || output->write(chunk) != chunk.size()) {
            reply->abort();
            return;
        }
        progress(output->size());
    };
    QObject::connect(reply, &QNetworkReply::readyRead, reply, append);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, output, append, done = std::move(done)] {
        reply->deleteLater();
        if (reply->isOpen())
            append();
        output->close();
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (!ok)
            qWarning("下载安装包没成：%s", qUtf8Printable(reply->errorString()));
        done(ok);
    });
}

// ---- Updater ----

Updater::Updater(Options options, UpdateSource &source, std::function<bool()> install, QObject *parent)
    : QObject(parent)
    , options_(std::move(options))
    , source_(source)
    , install_(std::move(install))
{
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this] { check(false); });

    QDir().mkpath(directory());
    // 上次启动了安装：装上的是那个版本就是装好了，不是就是没装上
    const QJsonObject record = rc::readJsonFile(recordFile());
    if (!record.isEmpty()) {
        const QString version = record.value(QLatin1StringView("version")).toString();
        if (QVersionNumber::fromString(version) == options_.version) {
            qInfo("已经更新到 %s", qUtf8Printable(version));
            justUpdated_ = true;
        } else {
            qWarning("上次装的新版本 %s 没换上，现在还是 %s", qUtf8Printable(version), qUtf8Printable(options_.version.toString()));
            state_.failed = QStringLiteral("install");
        }
        QFile::remove(recordFile());
    }
    // 下载的安装包用过了（或者没下载完）：删掉
    for (const QString &name : QDir(directory()).entryList({QFileInfo(options_.installer).fileName() + u'*'}, QDir::Files))
        QFile::remove(QDir(directory()).filePath(name));
}

void Updater::start()
{
    qInfo("程序的版本 %s，%d 分钟后问有没有新版本", qUtf8Printable(options_.version.toString()), options_.firstCheckMs / 60000);
    timer_.start(options_.firstCheckMs);
}

void Updater::checkNow()
{
    if (checking_ || state_.phase != u"idle")
        return;
    rc::protocol::UpdateState state = state_;
    state.phase = QStringLiteral("checking");
    state.checked.clear();
    setState(state);
    check(true);
}

void Updater::check(bool asked)
{
    timer_.start(options_.checkMs);
    if (checking_ || (state_.phase != u"idle" && state_.phase != u"checking"))
        return;
    checking_ = true;
    source_.latest(options_.platform, [this, asked](bool ok, std::optional<QJsonObject> release) {
        checking_ = false;
        if (release)
            received(*release);
        else if (ok) {
            // 还没发布过：没有新版本
            release_.reset();
            rc::protocol::UpdateState state = state_;
            state.available.clear();
            setState(state);
        }
        if (!asked)
            return;
        rc::protocol::UpdateState state = state_;
        state.phase = QStringLiteral("idle");
        state.checked = !ok ? QStringLiteral("failed") : state.available.isEmpty() ? QStringLiteral("upToDate") : QString();
        setState(state);
    });
}

void Updater::received(const QJsonObject &json)
{
    Release release{json.value(QLatin1StringView("version")).toString(), json.value(QLatin1StringView("size")).toInteger(),
                    json.value(QLatin1StringView("sha256")).toString().toLatin1(),
                    json.value(QLatin1StringView("url")).toString()};
    const QByteArray signature = QByteArray::fromBase64(json.value(QLatin1StringView("signature")).toString().toLatin1());
    const QVersionNumber number = QVersionNumber::fromString(release.version);
    if (number.isNull() || number <= options_.version) {
        release_.reset();
        rc::protocol::UpdateState state = state_;
        state.available.clear();
        state.failed.clear();
        setState(state);
        return;
    }
    // 服务器说的这几样是不是作者发布的：签名不对的不告诉界面
    if (release.size <= 0 || !release.path.startsWith(UpdatesPath)
        || !rc::release::signatureValid(options_.publicKey,
                                        rc::release::text(options_.platform, release.version, release.size, release.sha256),
                                        signature)) {
        qWarning("服务器上的新版本 %s 签名不对，不要", qUtf8Printable(release.version));
        return;
    }
    if (state_.available != release.version)
        qInfo("有新版本 %s", qUtf8Printable(release.version));
    rc::protocol::UpdateState state = state_;
    state.available = release.version;
    release_ = std::move(release);
    setState(state);
}

void Updater::installNow()
{
    if (!release_ || state_.phase != u"idle")
        return;
    qInfo("下载新版本 %s", qUtf8Printable(release_->version));
    setState({release_->version, QStringLiteral("downloading"), 0, {}});
    const QString partial = options_.installer + QStringLiteral(".partial");
    const qint64 size = release_->size;
    source_.download(release_->path, partial, size,
                     [this, size](qint64 received) {
                         rc::protocol::UpdateState state = state_;
                         state.progress = double(received) / double(size);
                         // 每多 1% 才告诉界面一次
                         if (int(state.progress * 100) != int(state_.progress * 100))
                             setState(state);
                     },
                     [this, partial](bool ok) {
                         // 下载下来的和签了名的那个一样大、sha256 一样，才换成要装的那个
                         bool valid = ok && QFileInfo(partial).size() == release_->size
                                      && fileSha256(partial) == release_->sha256;
                         if (valid) {
                             QFile::remove(options_.installer);
                             valid = QFile::rename(partial, options_.installer);
                         }
                         if (!valid)
                             QFile::remove(partial);
                         downloaded(valid);
                     });
}

void Updater::downloaded(bool ok)
{
    if (!ok) {
        qWarning("新版本 %s 没下载对（大小或 sha256 对不上）", qUtf8Printable(release_->version));
        setState({release_->version, QStringLiteral("idle"), 0, QStringLiteral("download")});
        return;
    }
    qInfo("新版本 %s 下载好了、核对过了，开始安装", qUtf8Printable(release_->version));
    rc::writeJsonFile(recordFile(), {{QStringLiteral("version"), release_->version}});
    setState({release_->version, QStringLiteral("installing"), 1, {}});
    if (!install_()) {
        qWarning("安装程序启动不了");
        QFile::remove(recordFile());
        setState({release_->version, QStringLiteral("idle"), 0, QStringLiteral("install")});
    }
    // 启动了：安装程序会停掉、换掉后台服务和界面程序
}

void Updater::setState(rc::protocol::UpdateState state)
{
    if (state == state_)
        return;
    state_ = std::move(state);
    emit stateChanged();
}

QString Updater::directory() const
{
    return QFileInfo(options_.installer).absolutePath();
}

QString Updater::recordFile() const
{
    return QDir(directory()).filePath(QStringLiteral("update.json"));
}
