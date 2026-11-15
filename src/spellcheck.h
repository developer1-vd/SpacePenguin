#pragma once

#include <QStringList>

namespace spacepenguin {

/**
 * Spell checking in text boxes, with Chromium's Hunspell dictionaries (.bdic
 * files). QtWebEngine reads them from a single folder, but systems keep them in
 * several (Flatpak, for one, keeps a folder per language), so prepare() links
 * every dictionary it finds into one folder of SpacePenguin's own and points
 * QtWebEngine there. Words are checked in the person's own languages, as far as
 * dictionaries for them are installed; with none, spell checking stays off.
 */
namespace SpellCheck {

/** Folders that may hold dictionaries, most specific first. */
QStringList searchDirectories();

/** Gathers the dictionaries into @p folder and sets QTWEBENGINE_DICTIONARIES_PATH;
 *  call before the first web page is created. Returns the languages found. */
QStringList prepare(const QString &folder);

/** The languages there are dictionaries for (en_US, de_DE, …), after prepare(). */
QStringList availableLanguages();

/**
 * Of @p available, the dictionaries for @p uiLanguages (as QLocale::uiLanguages()
 * gives them: "en-GB", "de"), in that order: an exact match, or else one for the
 * same language. At most three, as Chromium checks every word against each.
 */
QStringList chooseLanguages(const QStringList &uiLanguages, const QStringList &available);

/** The dictionaries to use here: chooseLanguages() for this system's languages. */
QStringList languagesForThisSystem();

/** "English (United States), Deutsch (Deutschland)", for the settings. */
QString describe(const QStringList &languages);

}

}
