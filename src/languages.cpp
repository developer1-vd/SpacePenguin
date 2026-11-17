#include "languages.h"

#include <QDir>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace spacepenguin::Languages {

QString normalizeTag(const QString &text)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z]{2,3}([-_][A-Za-z0-9]{2,8})*$"));
    const QString trimmed = text.trimmed();
    if (!valid.match(trimmed).hasMatch())
        return QString();
    QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[-_]")));
    parts[0] = parts[0].toLower();
    for (int i = 1; i < parts.size(); ++i) {
        // Regions in capitals (BR, 419 stays), scripts in title case (Hant).
        if (parts[i].size() == 4)
            parts[i] = parts[i].left(1).toUpper() + parts[i].mid(1).toLower();
        else
            parts[i] = parts[i].toUpper();
    }
    return parts.join(QLatin1Char('-'));
}

QStringList systemLanguages()
{
    QStringList tags;
    for (const QString &language : QLocale::system().uiLanguages()) {
        const QString tag = normalizeTag(language);
        if (!tag.isEmpty() && !tags.contains(tag) && tag != QLatin1String("C"))
            tags << tag;
    }
    if (tags.isEmpty())
        tags << QStringLiteral("en-US");
    return tags;
}

QStringList websiteLanguages(const QStringList &chosen)
{
    QStringList tags;
    for (const QString &language : chosen) {
        const QString tag = normalizeTag(language);
        if (!tag.isEmpty() && !tags.contains(tag))
            tags << tag;
    }
    if (tags.isEmpty())
        tags = systemLanguages();
    QStringList result;
    for (int i = 0; i < tags.size(); ++i) {
        if (!result.contains(tags[i]))
            result << tags[i];
        // The plain language after the last regional form of it in the list.
        const QString base = tags[i].section(QLatin1Char('-'), 0, 0);
        bool laterSameLanguage = false;
        for (int j = i + 1; j < tags.size(); ++j)
            laterSameLanguage = laterSameLanguage || tags[j].section(QLatin1Char('-'), 0, 0) == base;
        if (!laterSameLanguage && !result.contains(base))
            result << base;
    }
    return result;
}

QString acceptLanguage(const QStringList &chosen)
{
    return websiteLanguages(chosen).join(QLatin1Char(','));
}

QString nativeName(const QString &tag)
{
    const QLocale locale(QString(tag).replace(QLatin1Char('-'), QLatin1Char('_')));
    if (locale.language() == QLocale::C)
        return tag;
    QString name = locale.nativeLanguageName();
    if (name.isEmpty())
        name = QLocale::languageToString(locale.language());
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    // "Français suisse" and "American English" already say where; "Deutsch" for
    // de-CH does not.
    const QString plainName = QLocale(locale.language()).nativeLanguageName();
    const bool namesRegion = !plainName.isEmpty() && locale.nativeLanguageName().compare(plainName, Qt::CaseInsensitive) != 0;
    if (tag.contains(QLatin1Char('-')) && !namesRegion) {
        QString region = locale.nativeTerritoryName();
        if (region.isEmpty())
            region = QLocale::territoryToString(locale.territory());
        if (!region.isEmpty())
            name += QStringLiteral(" (%1)").arg(region);
    }
    return name;
}

QList<Language> allLanguages()
{
    QList<Language> languages;
    QSet<QString> seen;
    const QList<QLocale> locales =
        QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
    for (const QLocale &locale : locales) {
        if (locale.language() == QLocale::C)
            continue;
        const QString full = normalizeTag(locale.name());
        const QString plain = full.section(QLatin1Char('-'), 0, 0);
        for (const QString &tag : {plain, full}) {
            if (tag.isEmpty() || seen.contains(tag))
                continue;
            seen.insert(tag);
            languages.append({tag, nativeName(tag)});
        }
    }
    std::sort(languages.begin(), languages.end(), [](const Language &a, const Language &b) {
        const int order = QString::localeAwareCompare(a.name, b.name);
        return order != 0 ? order < 0 : a.tag < b.tag;
    });
    return languages;
}

QStringList appLanguages()
{
    QStringList tags{QStringLiteral("en")};
    const QStringList files = QDir(QStringLiteral(":/i18n")).entryList({QStringLiteral("spacepenguin_*.qm")});
    for (const QString &file : files) {
        const QString tag = normalizeTag(file.mid(13, file.size() - 13 - 3));
        if (!tag.isEmpty() && !tags.contains(tag))
            tags << tag;
    }
    return tags;
}

}
