#include "theme.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace spacepenguin {

QList<Theme::Mode> Theme::modes()
{
    return {Mode::System, Mode::Light, Mode::Dark};
}

QString Theme::modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::System:
        break;
    }
    return QStringLiteral("system");
}

QString Theme::modeLabel(Mode mode)
{
    switch (mode) {
    case Mode::Light:
        return QCoreApplication::translate("Theme", "Light");
    case Mode::Dark:
        return QCoreApplication::translate("Theme", "Dark");
    case Mode::System:
        break;
    }
    return QCoreApplication::translate("Theme", "Follow system");
}

Theme::Mode Theme::modeFromKey(const QString &key)
{
    if (key == QLatin1String("light"))
        return Mode::Light;
    if (key == QLatin1String("dark"))
        return Mode::Dark;
    return Mode::System;
}

Theme::Mode Theme::storedMode()
{
    const QString key = QSettings().value(QStringLiteral("appearance/theme"),
                                          QStringLiteral("system")).toString();
    return modeFromKey(key);
}

void Theme::storeMode(Mode mode)
{
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/theme"), modeKey(mode));
}

void Theme::apply(Mode mode)
{
    QStyleHints *hints = QGuiApplication::styleHints();
    if (!hints)
        return;

    switch (mode) {
    case Mode::Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Mode::Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case Mode::System:
        hints->unsetColorScheme();
        break;
    }
}

Theme::Mode Theme::effectiveMode()
{
    QStyleHints *hints = QGuiApplication::styleHints();
    if (!hints || hints->colorScheme() == Qt::ColorScheme::Unknown)
        return Mode::System;
    return hints->colorScheme() == Qt::ColorScheme::Dark ? Mode::Dark : Mode::Light;
}

}