#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace spacepenguin {

/**
 * Languages people choose for themselves rather than take from the system: the
 * one SpacePenguin speaks, the ones websites are asked for (Accept-Language and
 * navigator.languages), and the spelling dictionaries.
 */
namespace Languages {

/** A language and its own name for itself ("de-CH", "Deutsch (Schweiz)"). */
struct Language {
    QString tag;
    QString name;
};

/** "pt_BR", "pt-br" or "PT-BR" as a BCP 47 tag ("pt-BR"); empty if it is none. */
QString normalizeTag(const QString &text);

/** The system's languages as tags, most preferred first, without repeats. */
QStringList systemLanguages();

/**
 * What websites are asked for: @p chosen if any, else the system's languages.
 * Each regional language is followed by its plain one ("fr-CH, fr"), as other
 * browsers do, so a site with French but no Swiss French still offers French.
 */
QStringList websiteLanguages(const QStringList &chosen);
/** The same as one Accept-Language value ("fr-CH,fr,en"). */
QString acceptLanguage(const QStringList &chosen);

/** Every language and region Qt knows, sorted by name, for picking one. */
QList<Language> allLanguages();
/** The language's own name for itself, e.g. "Português (Brasil)". */
QString nativeName(const QString &tag);

/** The languages SpacePenguin itself is translated into ("en" always). */
QStringList appLanguages();

}

}
