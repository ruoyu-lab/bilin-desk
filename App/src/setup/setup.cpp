#include "setup.h"

#include <QCoreApplication>
#include <QDir>
#include <QJSEngine>
#include <QThread>

namespace {

Setup *instance = nullptr;

} // namespace

namespace setup_platform {

std::optional<Request> requestFromArguments(const QStringList &arguments)
{
    const qsizetype setup = arguments.indexOf(QStringLiteral("--setup"));
    if (setup < 0)
        return std::nullopt;
    Request request;
    request.mode = arguments.value(setup + 1) == u"uninstall" ? Request::Mode::Uninstall : Request::Mode::Install;
    const qsizetype target = arguments.indexOf(QStringLiteral("--target"));
    if (target >= 0)
        request.target = arguments.value(target + 1);
    const qsizetype data = arguments.indexOf(QStringLiteral("--data"));
    if (data >= 0)
        request.data = arguments.value(data + 1);
    request.quiet = arguments.contains(QStringLiteral("--quiet"));
    return request;
}

} // namespace setup_platform

Setup::Setup(std::optional<setup_platform::Request> request, QObject *parent)
    : QObject(parent)
    , request_(request.value_or(setup_platform::Request{setup_platform::Request::Mode::Uninstall, {}, {}}))
    , installedPath_(setup_platform::installedPath())
    , desktopShortcut_(installedPath_.isEmpty() || setup_platform::hasDesktopShortcut())
{
    // 升级时装回原来的地方
    if (request_.mode == setup_platform::Request::Mode::Install)
        installPath_ = installedPath_.isEmpty() ? setup_platform::defaultInstallPath() : installedPath_;
    else
        installPath_ = request_.target.isEmpty() ? installedPath_ : request_.target;
}

void Setup::setInstance(Setup *setup)
{
    instance = setup;
}

Setup *Setup::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

QString Setup::mode() const
{
    return request_.mode == setup_platform::Request::Mode::Install ? QStringLiteral("install") : QStringLiteral("uninstall");
}

void Setup::start()
{
    if (status_ == u"busy" || status_ == u"done")
        return;
    status_ = QStringLiteral("busy");
    error_.clear();
    setProgress(0);
    emit changed();

    setup_platform::Request request = request_;
    request.target = installPath_;
    const std::vector<SetupStep> steps =
        request.mode == setup_platform::Request::Mode::Install
            ? setup_platform::installSteps(QCoreApplication::applicationDirPath(), installPath_, desktopShortcut_)
            : setup_platform::uninstallSteps(request);
    // 复制文件要好几秒：在后台线程上做，进度和结果搬回来
    QThread *worker = QThread::create([this, steps] {
        const QString error = runSetupSteps(steps, [this](double total) {
            QMetaObject::invokeMethod(this, [this, total] { setProgress(total); }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(this, [this, error] { done(error); }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void Setup::setFolder(const QUrl &folder)
{
    // 装到选的文件夹里的 Bilin 子目录：选了 D:\ 不会把文件撒在盘的最上层
    installPath_ = QDir::toNativeSeparators(QDir(folder.toLocalFile()).filePath(QStringLiteral("Bilin")));
    emit changed();
}

void Setup::setDesktopShortcut(bool desktopShortcut)
{
    if (desktopShortcut == desktopShortcut_ || status_ == u"busy")
        return;
    desktopShortcut_ = desktopShortcut;
    emit changed();
}

void Setup::openInstalled()
{
    setup_platform::openInstalled(installPath_);
}

void Setup::setProgress(double progress)
{
    if (progress == progress_)
        return;
    progress_ = progress;
    emit progressChanged();
}

void Setup::done(const QString &error)
{
    error_ = error;
    status_ = error.isEmpty() ? QStringLiteral("done") : QStringLiteral("failed");
    emit changed();
    if (error.isEmpty() && request_.mode == setup_platform::Request::Mode::Uninstall)
        emit finished();
}
