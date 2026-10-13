#include "appearance.h"

#include <QGuiApplication>
#include <QJSEngine>
#include <QSettings>
#include <QStyleHints>

namespace {

Appearance *instance = nullptr;

constexpr QLatin1StringView SchemeKey("colorScheme");

} // namespace

Appearance::Appearance(QObject *parent)
    : QObject(parent)
    , scheme_(QSettings().value(SchemeKey, QStringLiteral("system")).toString())
{
    // 跟随系统时，系统换了跟着换
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &Appearance::changed);
    apply();
}

void Appearance::setInstance(Appearance *appearance)
{
    instance = appearance;
}

Appearance *Appearance::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(instance);
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void Appearance::setScheme(const QString &scheme)
{
    if (scheme == scheme_ || (scheme != u"system" && scheme != u"light" && scheme != u"dark"))
        return;
    scheme_ = scheme;
    QSettings().setValue(SchemeKey, scheme_);
    apply();
    emit changed();
}

bool Appearance::dark() const
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void Appearance::apply()
{
    QGuiApplication::styleHints()->setColorScheme(scheme_ == u"light"  ? Qt::ColorScheme::Light
                                                  : scheme_ == u"dark" ? Qt::ColorScheme::Dark
                                                                       : Qt::ColorScheme::Unknown);
}
