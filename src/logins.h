#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

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
 * On Windows a file in the profile's folder, encrypted with the user's Windows
 * account (DPAPI), so another account, or the disk in another computer, cannot read
 * it. On macOS the login Keychain, and on Linux and the BSDs the desktop's keyring
 * (the Secret Service: GNOME Keyring, KWallet…), hold them, encrypted and unlocked
 * with the user's login; a file from before moves in there. With no keyring they
 * stay in a file only the user's account may read. storageDescription() says which.
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
    /** Where the logins are kept now, for the manager to tell people. */
    QString storageDescription() const;

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
    QString keyringKey() const;

    QString m_path;
    bool m_inKeyring = false; // kept in the system keyring, not the file
    bool m_readOnly = false;  // the keyring stayed locked: do not overwrite it
    QList<Login> m_logins;
    QSet<QString> m_never;
};

}
