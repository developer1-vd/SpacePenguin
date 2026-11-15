#include "spellcheck.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QStandardPaths>

namespace spacepenguin::SpellCheck {
namespace {

QStringList &found()
{
    static QStringList languages;
    return languages;
}

QStringList dictionariesIn(const QString &directory)
{
    QStringList files;
    QDirIterator it(directory, {QStringLiteral("*.bdic")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        files.append(it.next());
    return files;
}

} // namespace

QStringList searchDirectories()
{
    QStringList directories;
    const QByteArray fromEnvironment = qgetenv("QTWEBENGINE_DICTIONARIES_PATH");
    if (!fromEnvironment.isEmpty())
        directories << QString::fromLocal8Bit(fromEnvironment);
    const QString appDir = QCoreApplication::applicationDirPath();
    directories << appDir + QStringLiteral("/qtwebengine_dictionaries")
                << appDir + QStringLiteral("/../Resources/qtwebengine_dictionaries")       // macOS bundle
                << appDir + QStringLiteral("/../share/qtwebengine_dictionaries")           // AppImage, /usr/local
                << appDir + QStringLiteral("/../share/spacepenguin/qtwebengine_dictionaries")
                << QLibraryInfo::path(QLibraryInfo::DataPath) + QStringLiteral("/qtwebengine_dictionaries");
    // Flatpak (the QtWebEngine BaseApp: share/locale/<language>/qtwebengine_dictionaries)
    // and distributions' Qt packages.
    QStringList shares = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    shares << QStringLiteral("/app/share") << QStringLiteral("/usr/share") << QStringLiteral("/usr/local/share");
    for (const QString &share : std::as_const(shares)) {
        directories << share + QStringLiteral("/qt6/qtwebengine_dictionaries")
                    << share + QStringLiteral("/qtwebengine_dictionaries")
                    << share + QStringLiteral("/locale");
    }
    directories.removeDuplicates();
    return directories;
}

QStringList prepare(const QString &folder)
{
    QDir target(folder);
    target.mkpath(QStringLiteral("."));
    // Last time's links may point at dictionaries since removed.
    for (const QFileInfo &old : target.entryInfoList({QStringLiteral("*.bdic")}, QDir::Files | QDir::System))
        QFile::remove(old.absoluteFilePath());

    QStringList languages;
    for (const QString &directory : searchDirectories()) {
        if (QDir(directory).canonicalPath() == target.canonicalPath() || !QFileInfo(directory).isDir())
            continue;
        for (const QString &file : dictionariesIn(directory)) {
            const QString name = QFileInfo(file).fileName();
            const QString language = QFileInfo(file).completeBaseName();
            if (languages.contains(language))
                continue; // the more specific folder's copy wins
#ifdef Q_OS_WIN
            const bool ok = QFile::copy(file, target.filePath(name)); // links need rights there
#else
            const bool ok = QFile::link(file, target.filePath(name));
#endif
            if (ok)
                languages << language;
        }
    }
    found() = languages;
    qputenv("QTWEBENGINE_DICTIONARIES_PATH", QFile::encodeName(target.absolutePath()));
    return languages;
}

QStringList availableLanguages()
{
    return found();
}

QStringList chooseLanguages(const QStringList &uiLanguages, const QStringList &available)
{
    QStringList chosen;
    const auto add = [&chosen](const QString &language) {
        if (!language.isEmpty() && !chosen.contains(language) && chosen.size() < 3)
            chosen << language;
    };
    for (QString wanted : uiLanguages) {
        wanted.replace(QLatin1Char('-'), QLatin1Char('_'));
        const QString language = wanted.section(QLatin1Char('_'), 0, 0);
        // "en" after "en-US": English is checked already.
        const bool bare = !wanted.contains(QLatin1Char('_'));
        if (bare && std::any_of(chosen.cbegin(), chosen.cend(), [&language](const QString &c) {
                return c.section(QLatin1Char('_'), 0, 0).compare(language, Qt::CaseInsensitive) == 0;
            }))
            continue;
        QString match;
        for (const QString &candidate : available) {
            if (candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                match = candidate;
                break;
            }
        }
        if (match.isEmpty()) {
            for (const QString &candidate : available) {
                if (candidate.compare(language, Qt::CaseInsensitive) == 0
                    || candidate.startsWith(language + QLatin1Char('_'), Qt::CaseInsensitive)) {
                    match = candidate;
                    break;
                }
            }
        }
        add(match);
    }
    return chosen;
}

QStringList languagesForThisSystem()
{
    return chooseLanguages(QLocale::system().uiLanguages(), availableLanguages());
}

QString describe(const QStringList &languages)
{
    QStringList names;
    for (const QString &language : languages) {
        const QString name = QLocale(language).nativeLanguageName();
        names << (name.isEmpty() ? language : name);
    }
    // Two kinds of the same language ("English" and "English") say where from.
    for (int i = 0; i < names.size(); ++i) {
        if (names.count(names.at(i)) > 1 && languages.at(i).contains(QLatin1Char('_')))
            names[i] = QStringLiteral("%1 (%2)").arg(names.at(i), QLocale(languages.at(i)).nativeTerritoryName());
    }
    return names.join(QStringLiteral(", "));
}

}
