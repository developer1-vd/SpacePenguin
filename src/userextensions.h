#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QWebEngineProfile;
class QWebEngineScript;

namespace spacepenguin {

struct UserScript {
    QString id;
    QString name;
    QString filePath;
    QStringList matchPatterns;   // @match and @include
    QStringList excludePatterns; // @exclude and @exclude-match
    QStringList libraries;       // @require: libraries it runs after
    QString runAt;               // @run-at: document-start, document-end or document-idle
    QString version;
    QString description;
    bool noFrames = false;
    bool enabled = true;
};

class UserScripts : public QObject
{
    Q_OBJECT

public:
    explicit UserScripts(QWebEngineProfile *profile, QObject *parent = nullptr);

    QString directory() const { return m_directory; }
    void setDirectory(const QString &path);

    QList<UserScript> scripts() const { return m_scripts; }

    bool addScript(const QString &filePath, QString *error = nullptr);
    bool removeScript(const QString &id);
    bool setScriptEnabled(const QString &id, bool enabled);
    bool isEnabled(const QString &id) const;

    void apply();
    void reloadFromDisk();

    static UserScript parse(const QString &filePath, const QString &contents);
    static QStringList parseMatchPatterns(const QString &contents);
    static QString parseName(const QString &contents, const QString &fallback);
    static QStringList parseRequires(const QString &contents);
    /**
     * The script wrapped to run only on its sites, with the GM_ functions that
     * script managers such as Tampermonkey give scripts: settings storage, styles,
     * menu commands (see menuCommandsScript) and the like.
     */
    static QString buildInjectedSource(const UserScript &script, const QString &source);

    /** Where the copy of an @require library is kept once downloaded. */
    QString requireFile(const QUrl &url) const;
    /** The @require libraries of @p contents that have not been downloaded yet. */
    QList<QUrl> missingRequires(const QString &contents) const;

    /**
     * JavaScript, run in the page's ApplicationWorld, that lists the menu commands
     * the page's scripts registered, as [[id, script name, command], …].
     */
    static QString menuCommandsScript();
    /** JavaScript that runs the menu command @p id. */
    static QString runMenuCommandScript(int id);

    /**
     * Scripts whose name starts with this prefix belong to the browser, not to the
     * user, and are preserved when the user script collection is rebuilt.
     */
    static QString reservedPrefix();

signals:
    void changed();

private:
    QWebEngineProfile *m_profile = nullptr;
    QList<UserScript> m_scripts;
    QString m_directory;
};

}