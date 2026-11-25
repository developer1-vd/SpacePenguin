#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>

namespace spacepenguin {

/**
 * One account in the authenticator: what a site's "set up two-factor
 * authentication" page hands over, usually as an otpauth:// link or QR code.
 */
struct OtpAccount {
    enum class Type { Totp, Hotp };
    enum class Algorithm { Sha1, Sha256, Sha512 };

    QString id;      // stable, chosen when added
    QString issuer;  // "GitHub"
    QString name;    // the account at the issuer, "penguin@example.com"
    QByteArray secret; // the shared key, decoded from base32
    Type type = Type::Totp;
    Algorithm algorithm = Algorithm::Sha1;
    int digits = 6;
    int period = 30;      // seconds, for TOTP
    quint64 counter = 0;  // the next counter, for HOTP
    QString site;         // the origin it was added on, to offer its codes there
    QDateTime created;

    bool isValid() const { return !secret.isEmpty(); }
};

/**
 * One-time passwords: HOTP (RFC 4226) and TOTP (RFC 6238), with the base32 keys
 * and otpauth:// links (Google Authenticator's "Key Uri Format") sites use.
 */
namespace Otp {

/** RFC 4648 base32, as sites show keys: case, spaces, dashes and padding ignored. Empty when invalid. */
QByteArray base32Decode(const QString &text);
QString base32Encode(const QByteArray &data);

/** The HOTP code for @p counter, @p digits long, zero-padded. */
QString hotp(const QByteArray &secret, quint64 counter, int digits = 6,
             OtpAccount::Algorithm algorithm = OtpAccount::Algorithm::Sha1);
/** The TOTP code at @p unixTime (seconds). */
QString totp(const QByteArray &secret, qint64 unixTime, int period = 30, int digits = 6,
             OtpAccount::Algorithm algorithm = OtpAccount::Algorithm::Sha1);

/** The account's code now (TOTP) or for its next counter (HOTP). */
QString code(const OtpAccount &account, qint64 unixTime);
/** Seconds the TOTP code at @p unixTime stays valid. */
int secondsLeft(const OtpAccount &account, qint64 unixTime);

/**
 * Reads an otpauth://totp/... or otpauth://hotp/... link. On a bad link the
 * account is invalid and @p error says why, in words for people.
 */
OtpAccount parseUri(const QString &uri, QString *error = nullptr);
/** The otpauth:// link for @p account, to move it to another app. */
QString toUri(const OtpAccount &account);

/**
 * A key typed in by hand: base32 as sites show it. On a bad key the account is
 * invalid and @p error says why.
 */
OtpAccount fromKey(const QString &issuer, const QString &name, const QString &key, QString *error = nullptr);

/** Whether the account belongs to the site at @p url: added there, or named like it. */
bool matchesSite(const OtpAccount &account, const QUrl &url);

QString algorithmName(OtpAccount::Algorithm algorithm);

}

}
