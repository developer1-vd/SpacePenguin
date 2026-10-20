#pragma once

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

class QLineEdit;
class QTreeWidget;

namespace spacepenguin {

struct Login {
    QString origin; // scheme://host[:port], as QUrl::adjusted(RemovePath...) gives it
    QString username;
    QString password;
    QDateTime created;
    QDateTime lastUsed;
};

/**
 * The logins people chose to save, per site.
 *
 * Kept in the profile's folder. On Windows the file is encrypted with the user's
 * Windows account (DPAPI), so another account, or the disk in another computer,
 * cannot read it. Elsewhere it is a file only the user's account may read, as the
 * system's keyring is not something every desktop has; storageDescription() says
 * which, for the manager to tell people.
 *
 * Pages never get a login unasked: the window fills one in only when the person
 * picks it.
 */
class LoginStore : public QObject
{
    Q_OBJECT

public:
    explicit LoginStore(const QString &directory, QObject *parent = nullptr);

    /** The origin logins are filed under for a page at @p url; empty for non-web pages. */
    static QString originOf(const QUrl &url);
    static QString storageDescription();

    QList<Login> logins() const { return m_logins; }
    QList<Login> loginsFor(const QString &origin) const;
    bool contains(const QString &origin, const QString &username, const QString &password) const;

    /** Saves a login, replacing the password of one with the same site and name. */
    void save(const QString &origin, const QString &username, const QString &password);
    /** Adds logins brought from another browser; returns how many were new or changed. */
    int importLogins(const QList<Login> &logins);
    void remove(const QString &origin, const QString &username);
    void markUsed(const QString &origin, const QString &username);

    bool isNeverSaved(const QString &origin) const { return m_never.contains(origin); }
    void setNeverSave(const QString &origin, bool never);
    QStringList neverSaved() const;

    /**
     * Prefix of the console lines the capture script reports logins with: random
     * for each run, and known only to the script's isolated world, so a page cannot
     * fake a report.
     */
    static QString captureToken();
    /** Run in a page's isolated world: reports a submitted login as a console line. */
    static QString captureScript(const QString &token);
    /** Run in a page's isolated world: fills a login into its sign-in form. */
    static QString fillScript(const QString &username, const QString &password);

signals:
    void changed();

private:
    void load();
    void store() const;

    QString m_path;
    QList<Login> m_logins;
    QSet<QString> m_never;
};

/** Settings › Saved logins: see, copy and delete them. */
class LoginsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginsDialog(LoginStore *store, QWidget *parent = nullptr);

private:
    void rebuild();

    LoginStore *m_store;
    QLineEdit *m_search = nullptr;
    QTreeWidget *m_list = nullptr;
};

}
