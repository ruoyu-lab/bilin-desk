#include "autostart.h"

#include <QGuiApplication>
#include <QJSEngine>
#include <QSettings>

using login_item::Status;

namespace {

Autostart *instance = nullptr;

// 偏好设置里记着：第一次运行时已经替用户打开过了
constexpr QLatin1StringView DefaultAppliedKey("autostartDefaultApplied");

} // namespace

Autostart::Autostart(QObject *parent)
    : QObject(parent)
    , status_(login_item::status())
{
    login_item::whenLaunched([this](bool atLogin) {
        if (atLogin)
            qInfo("登录时由系统启动：不弹窗口");
        emit launched(atLogin);
    });
    // 用户可能在系统设置里改了：回到程序时重新读
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            refresh();
    });
    // 默认开：第一次运行时替用户打开。只这一次，之后用户关了、系统没让开，都不再自动去开
    QSettings settings;
    if (!settings.value(DefaultAppliedKey).toBool()) {
        settings.setValue(DefaultAppliedKey, true);
        setEnabled(true);
    }
}

void Autostart::setInstance(Autostart *autostart)
{
    instance = autostart;
}

Autostart *Autostart::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void Autostart::followService(std::function<bool()> get, std::function<void(bool)> set)
{
    followed_ = Followed{std::move(get), std::move(set)};
}

void Autostart::setEnabled(bool enabled)
{
    if (enabled != loginItemOn()) {
        qInfo("%s开机时自动启动", enabled ? "打开" : "关掉");
        login_item::setEnabled(enabled);
        status_ = login_item::status();
    }
    if (followed_ && enabled != followed_->get())
        followed_->set(enabled);
    // 要用户批准的由 approval 说明；别的没改成是系统或安全软件不让改
    refused_ = loginItemOn() != enabled && status_ != Status::NeedsApproval;
    // 没改成也通知：开关回到系统里的真实状态上
    emit changed();
}

QString Autostart::problem() const
{
    if (status_ == Status::NeedsApproval)
        return QStringLiteral("approval");
    return refused_ ? QStringLiteral("refused") : QString();
}

void Autostart::openSystemSettings()
{
    login_item::openSystemSettings();
}

void Autostart::refresh()
{
    const Status status = login_item::status();
    if (status == status_)
        return;
    status_ = status;
    refused_ = false;
    emit changed();
}
