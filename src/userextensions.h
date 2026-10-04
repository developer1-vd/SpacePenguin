#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QWebEngineProfile;
class QWebEngineScript;

namespace spacepenguin {

struct UserScript {
    QString id;
    QString name;
    QString filePath;
    QStringList matchPatterns;
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
    static QString buildInjectedSource(const UserScript &script, const QString &source);

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