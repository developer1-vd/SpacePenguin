#pragma once

#include "otp.h"

#include <QList>
#include <QObject>
#include <QString>

namespace spacepenguin {

/**
 * The authenticator's accounts: the keys for two-factor sign-in codes.
 *
 * Kept like saved logins (logins.h): on Windows a file encrypted with the
 * user's Windows account (DPAPI); on macOS, Linux and the BSDs the system
 * keyring; with no keyring a file only the user's account may read. The
 * window shows them, and the codes, only after the primary password.
 */
class AuthenticatorStore : public QObject
{
    Q_OBJECT

public:
    explicit AuthenticatorStore(const QString &directory, QObject *parent = nullptr);

    QList<OtpAccount> accounts() const { return m_accounts; }
    OtpAccount account(const QString &id) const;
    /** The accounts for the site at @p url, in the order they were added. */
    QList<OtpAccount> accountsFor(const QUrl &url) const;

    /**
     * Adds @p account and returns its id. One with the same key is replaced (the
     * site gave the same key again), keeping its id.
     */
    QString add(OtpAccount account);
    void remove(const QString &id);
    void rename(const QString &id, const QString &issuer, const QString &name);

    /** The account's code now; for HOTP the next code, and the counter moves on. */
    QString takeCode(const QString &id, qint64 unixTime);

    /**
     * Run in a page's isolated world: fills @p code into the page's one-time-code
     * box (autocomplete="one-time-code", or one named like it, or a row of
     * one-digit boxes). Returns whether it found one. @p code must be digits.
     */
    static QString fillScript(const QString &code);

    /** Where the accounts are kept now, for the page to tell people. */
    QString storageDescription() const;

signals:
    void changed();

private:
    void load();
    void store() const;
    QString keyringKey() const;

    QString m_path;
    bool m_inKeyring = false;
    bool m_readOnly = false;
    QList<OtpAccount> m_accounts;
};

}
