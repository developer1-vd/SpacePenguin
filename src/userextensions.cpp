#include "userextensions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace spacepenguin {
namespace {

QString metadataValue(const QString &contents, const QString &key, const QString &fallback = QString())
{
    const QStringList lines = contents.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1Char('/')))
            continue;

        const QString comment = trimmed.mid(2).trimmed();
        const int space = comment.indexOf(QLatin1Char(' '));
        if (space < 0)
            continue;

        if (comment.left(space) == QLatin1String("@") + key)
            return comment.mid(space + 1).trimmed();
    }

    return fallback;
}

QString matchPatternsToRegExp(const QStringList &patterns)
{
    QStringList sources;
    for (const QString &pattern : patterns) {
        QString escaped = QRegularExpression::escape(pattern);
        escaped.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
        sources.append(QStringLiteral("^(?:") + escaped + QStringLiteral(")$"));
    }

    if (sources.isEmpty())
        return QStringLiteral(".*");

    return sources.join(QLatin1Char('|'));
}

} // namespace

UserScripts::UserScripts(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
}

void UserScripts::setDirectory(const QString &path)
{
    m_directory = path;
    reloadFromDisk();
}

bool UserScripts::addScript(const QString &filePath, QString *error)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        if (error)
            *error = tr("No such file: %1").arg(filePath);
        return false;
    }

    if (info.suffix().compare(QLatin1String("js"), Qt::CaseInsensitive) != 0) {
        if (error)
            *error = tr("User scripts must be .js files.");
        return false;
    }

    if (m_directory.isEmpty()) {
        if (error)
            *error = tr("No extension directory is configured.");
        return false;
    }

    QDir().mkpath(m_directory);
    const QString target = QDir(m_directory).filePath(info.fileName());
    const bool alreadyInstalled = QFileInfo(target).absoluteFilePath() == info.absoluteFilePath();

    if (!alreadyInstalled) {
        if (QFile::exists(target)) {
            if (error)
                *error = tr("An extension named %1 already exists.").arg(info.fileName());
            return false;
        }

        if (!QFile::copy(info.absoluteFilePath(), target)) {
            if (error)
                *error =
                    tr("Could not copy %1 into the extension directory.").arg(info.fileName());
            return false;
        }
    }

    reloadFromDisk();
    return true;
}

bool UserScripts::removeScript(const QString &id)
{
    for (const UserScript &script : std::as_const(m_scripts)) {
        if (script.id != id)
            continue;

        QFile::remove(script.filePath);
        reloadFromDisk();
        return true;
    }

    return false;
}

bool UserScripts::setScriptEnabled(const QString &id, bool enabled)
{
    for (UserScript &script : m_scripts) {
        if (script.id != id)
            continue;

        script.enabled = enabled;
        apply();
        return true;
    }

    return false;
}

bool UserScripts::isEnabled(const QString &id) const
{
    for (const UserScript &script : m_scripts) {
        if (script.id == id)
            return script.enabled;
    }
    return false;
}

void UserScripts::reloadFromDisk()
{
    m_scripts.clear();

    if (m_directory.isEmpty())
        return;

    QDir directory(m_directory);
    const QStringList files = directory.entryList({QStringLiteral("*.js")}, QDir::Files,
                                                  QDir::Name);
    for (const QString &file : files) {
        QFile handle(directory.filePath(file));
        if (!handle.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString contents = QString::fromUtf8(handle.readAll());
        handle.close();
        m_scripts.append(parse(directory.filePath(file), contents));
    }

    apply();
}

void UserScripts::apply()
{
    if (!m_profile)
        return;

    QWebEngineScriptCollection *collection = m_profile->scripts();

    // Only drop the user scripts; browser-owned scripts share this collection.
    const QList<QWebEngineScript> existing = collection->toList();
    for (const QWebEngineScript &script : existing) {
        if (script.name().startsWith(reservedPrefix()))
            continue;
        collection->remove(script);
    }

    for (const UserScript &script : std::as_const(m_scripts)) {
        if (!script.enabled)
            continue;

        QFile handle(script.filePath);
        if (!handle.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString source = QString::fromUtf8(handle.readAll());
        handle.close();

        QWebEngineScript engineScript;
        engineScript.setName(script.name);
        engineScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
        engineScript.setWorldId(QWebEngineScript::ApplicationWorld);
        engineScript.setSourceCode(buildInjectedSource(script, source));
        collection->insert(engineScript);
    }

    emit changed();
}

UserScript UserScripts::parse(const QString &filePath, const QString &contents)
{
    UserScript script;
    script.filePath = filePath;
    script.id = QFileInfo(filePath).fileName();
    script.name = parseName(contents, script.id);
    script.matchPatterns = parseMatchPatterns(contents);
    return script;
}

QString UserScripts::parseName(const QString &contents, const QString &fallback)
{
    return metadataValue(contents, QStringLiteral("name"), fallback);
}

QStringList UserScripts::parseMatchPatterns(const QString &contents)
{
    QStringList patterns;
    const QString raw = metadataValue(contents, QStringLiteral("match"));
    for (const QString &pattern : raw.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = pattern.trimmed();
        if (!trimmed.isEmpty())
            patterns.append(trimmed);
    }
    return patterns;
}

QString UserScripts::reservedPrefix()
{
    return QStringLiteral("spacepenguin:");
}

QString UserScripts::buildInjectedSource(const UserScript &script, const QString &source)
{
    const QString patterns = matchPatternsToRegExp(script.matchPatterns);
    return QStringLiteral(
               "(function() {\n"
               "  var spHost = location.hostname;\n"
               "  var spPatterns = new RegExp(%1);\n"
               "  if (!spPatterns.test(spHost)) return;\n"
               "  try {\n%2\n"
               "  } catch (error) {\n"
               "    console.warn('SpacePenguin user script failed:', error);\n"
               "  }\n"
               "})();")
        .arg(patterns, source);
}

}