#include "otp.h"

#include <QCoreApplication>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QtEndian>

#include <cstring>

namespace spacepenguin::Otp {
namespace {

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// Shorter keys than this (40 bits) are too weak to be real; sites give 80 or more.
constexpr int kMinimumKeyBytes = 5;
constexpr int kMaximumKeyBytes = 1024;

QString tr(const char *text)
{
    return QCoreApplication::translate("Otp", text);
}

QCryptographicHash::Algorithm hashOf(OtpAccount::Algorithm algorithm)
{
    switch (algorithm) {
    case OtpAccount::Algorithm::Sha256:
        return QCryptographicHash::Sha256;
    case OtpAccount::Algorithm::Sha512:
        return QCryptographicHash::Sha512;
    case OtpAccount::Algorithm::Sha1:
        break;
    }
    return QCryptographicHash::Sha1;
}

// "GitHub", "git hub" and "GITHUB" alike: letters and digits, lower case.
QString simplified(const QString &text)
{
    QString out;
    for (const QChar c : text) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    }
    return out;
}

QString hostWithoutWww(const QString &host)
{
    return host.startsWith(QLatin1String("www.")) ? host.mid(4) : host;
}

bool checkKey(const QByteArray &secret, const QString &text, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (text.trimmed().isEmpty())
        return fail(tr("Enter the setup key the site shows."));
    if (secret.isEmpty())
        return fail(tr("That isn't a setup key: it can only have the letters A to Z and the digits 2 to 7."));
    if (secret.size() < kMinimumKeyBytes)
        return fail(tr("That setup key is too short. Check that you copied all of it."));
    if (secret.size() > kMaximumKeyBytes)
        return fail(tr("That setup key is too long."));
    return true;
}

} // namespace

QByteArray base32Decode(const QString &text)
{
    QByteArray out;
    quint64 buffer = 0;
    int bits = 0;
    int count = 0;
    for (const QChar c : text) {
        if (c.isSpace() || c == QLatin1Char('-') || c == QLatin1Char('='))
            continue;
        const char upper = c.toUpper().toLatin1();
        const char *found = upper ? std::strchr(kAlphabet, upper) : nullptr;
        if (!found)
            return {};
        buffer = (buffer << 5) | quint64(found - kAlphabet);
        bits += 5;
        ++count;
        if (bits >= 8) {
            bits -= 8;
            out.append(char((buffer >> bits) & 0xff));
        }
    }
    // A last group of 1, 3 or 6 characters cannot come from whole bytes.
    const int tail = count % 8;
    if (tail == 1 || tail == 3 || tail == 6)
        return {};
    return out;
}

QString base32Encode(const QByteArray &data)
{
    QString out;
    quint64 buffer = 0;
    int bits = 0;
    for (const char byte : data) {
        buffer = (buffer << 8) | quint8(byte);
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.append(QLatin1Char(kAlphabet[(buffer >> bits) & 31]));
        }
    }
    if (bits > 0)
        out.append(QLatin1Char(kAlphabet[(buffer << (5 - bits)) & 31]));
    return out;
}

QString hotp(const QByteArray &secret, quint64 counter, int digits, OtpAccount::Algorithm algorithm)
{
    digits = qBound(6, digits, 10);
    char message[8];
    qToBigEndian(counter, message);
    const QByteArray mac = QMessageAuthenticationCode::hash(QByteArray(message, 8), secret, hashOf(algorithm));
    // RFC 4226 5.3: dynamic truncation.
    const int offset = mac.at(mac.size() - 1) & 0x0f;
    const quint64 binary = (quint64(quint8(mac.at(offset)) & 0x7f) << 24) | (quint64(quint8(mac.at(offset + 1))) << 16)
        | (quint64(quint8(mac.at(offset + 2))) << 8) | quint64(quint8(mac.at(offset + 3)));
    quint64 modulus = 1;
    for (int i = 0; i < digits; ++i)
        modulus *= 10;
    return QStringLiteral("%1").arg(binary % modulus, digits, 10, QLatin1Char('0'));
}

QString totp(const QByteArray &secret, qint64 unixTime, int period, int digits, OtpAccount::Algorithm algorithm)
{
    period = qMax(1, period);
    const qint64 step = unixTime >= 0 ? unixTime / period : (unixTime - period + 1) / period;
    return hotp(secret, quint64(qMax<qint64>(0, step)), digits, algorithm);
}

QString code(const OtpAccount &account, qint64 unixTime)
{
    if (account.type == OtpAccount::Type::Hotp)
        return hotp(account.secret, account.counter, account.digits, account.algorithm);
    return totp(account.secret, unixTime, account.period, account.digits, account.algorithm);
}

int secondsLeft(const OtpAccount &account, qint64 unixTime)
{
    const int period = qMax(1, account.period);
    const int elapsed = int(((unixTime % period) + period) % period);
    return period - elapsed;
}

OtpAccount parseUri(const QString &uri, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return OtpAccount();
    };
    const QUrl url(uri.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.scheme().compare(QLatin1String("otpauth"), Qt::CaseInsensitive) != 0)
        return fail(tr("That isn't an authenticator link (otpauth://)."));

    OtpAccount account;
    const QString type = url.host().toLower();
    if (type == QLatin1String("totp"))
        account.type = OtpAccount::Type::Totp;
    else if (type == QLatin1String("hotp"))
        account.type = OtpAccount::Type::Hotp;
    else
        return fail(tr("This kind of authenticator code (%1) isn't supported.").arg(type));

    const QUrlQuery query(url);
    const auto value = [&query](const char *key) {
        return query.queryItemValue(QString::fromLatin1(key), QUrl::FullyDecoded).trimmed();
    };

    // The label is "Issuer:account" or just "account".
    QString label = url.path(QUrl::FullyDecoded);
    if (label.startsWith(QLatin1Char('/')))
        label.remove(0, 1);
    const qsizetype colon = label.indexOf(QLatin1Char(':'));
    QString labelIssuer;
    account.issuer = value("issuer");
    // With no "Issuer:" in front, a colon is part of the name.
    if (colon >= 0 && (account.issuer.isEmpty() || label.left(colon).trimmed() == account.issuer
                       || !account.issuer.contains(QLatin1Char(':')))) {
        labelIssuer = label.left(colon).trimmed();
        label = label.mid(colon + 1);
    }
    account.name = label.trimmed();
    if (account.issuer.isEmpty())
        account.issuer = labelIssuer;

    const QString key = value("secret");
    account.secret = base32Decode(key);
    QString keyError;
    if (!checkKey(account.secret, key, &keyError))
        return fail(key.isEmpty() ? tr("The link has no secret key.") : keyError);

    const QString algorithm = value("algorithm").toUpper();
    if (algorithm.isEmpty() || algorithm == QLatin1String("SHA1"))
        account.algorithm = OtpAccount::Algorithm::Sha1;
    else if (algorithm == QLatin1String("SHA256"))
        account.algorithm = OtpAccount::Algorithm::Sha256;
    else if (algorithm == QLatin1String("SHA512"))
        account.algorithm = OtpAccount::Algorithm::Sha512;
    else
        return fail(tr("The link uses an algorithm (%1) that isn't supported.").arg(algorithm));

    bool ok = true;
    if (const QString digits = value("digits"); !digits.isEmpty()) {
        account.digits = digits.toInt(&ok);
        if (!ok || account.digits < 6 || account.digits > 8)
            return fail(tr("The link asks for %1-digit codes; only 6, 7 or 8 are supported.").arg(digits));
    }
    if (const QString period = value("period"); !period.isEmpty()) {
        account.period = period.toInt(&ok);
        if (!ok || account.period < 10 || account.period > 300)
            return fail(tr("The link has a code lifetime (%1 seconds) that isn't supported.").arg(period));
    }
    if (account.type == OtpAccount::Type::Hotp) {
        const QString counter = value("counter");
        account.counter = counter.isEmpty() ? 0 : counter.toULongLong(&ok);
        if (!ok)
            return fail(tr("The link's counter isn't a number."));
    }
    if (error)
        error->clear();
    return account;
}

QString toUri(const OtpAccount &account)
{
    QUrl url;
    url.setScheme(QStringLiteral("otpauth"));
    url.setHost(account.type == OtpAccount::Type::Hotp ? QStringLiteral("hotp") : QStringLiteral("totp"));
    // "Issuer:name" in the label, as apps show it, unless a colon in the issuer would
    // split it in the wrong place; the issuer parameter carries it either way.
    const bool issuerInLabel = !account.issuer.isEmpty() && !account.issuer.contains(QLatin1Char(':'));
    url.setPath(QLatin1Char('/') + (issuerInLabel ? account.issuer + QLatin1Char(':') : QString()) + account.name);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("secret"), base32Encode(account.secret));
    if (!account.issuer.isEmpty())
        query.addQueryItem(QStringLiteral("issuer"), QString(account.issuer).replace(QLatin1Char('&'), QStringLiteral("%26")));
    query.addQueryItem(QStringLiteral("algorithm"), algorithmName(account.algorithm));
    query.addQueryItem(QStringLiteral("digits"), QString::number(account.digits));
    if (account.type == OtpAccount::Type::Hotp)
        query.addQueryItem(QStringLiteral("counter"), QString::number(account.counter));
    else
        query.addQueryItem(QStringLiteral("period"), QString::number(account.period));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

OtpAccount fromKey(const QString &issuer, const QString &name, const QString &key, QString *error)
{
    OtpAccount account;
    account.secret = base32Decode(key);
    if (!checkKey(account.secret, key, error))
        return OtpAccount();
    account.issuer = issuer.trimmed();
    account.name = name.trimmed();
    if (error)
        error->clear();
    return account;
}

bool matchesSite(const OtpAccount &account, const QUrl &url)
{
    const QString host = hostWithoutWww(url.host().toLower());
    if (host.isEmpty())
        return false;
    // Added on this site, or one of its subdomains (accounts.example.com and example.com).
    if (!account.site.isEmpty()) {
        const QString site = hostWithoutWww(QUrl(account.site).host().toLower());
        if (!site.isEmpty() && (host == site || host.endsWith(QLatin1Char('.') + site) || site.endsWith(QLatin1Char('.') + host)))
            return true;
    }
    // Named like it: "GitHub" for github.com, "Google" for accounts.google.com.
    const QString issuer = simplified(account.issuer);
    if (issuer.size() < 3)
        return false;
    QStringList labels = host.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (labels.size() > 1)
        labels.removeLast(); // the top-level domain
    for (const QString &label : labels) {
        if (simplified(label) == issuer)
            return true;
    }
    return false;
}

QString algorithmName(OtpAccount::Algorithm algorithm)
{
    switch (algorithm) {
    case OtpAccount::Algorithm::Sha256:
        return QStringLiteral("SHA256");
    case OtpAccount::Algorithm::Sha512:
        return QStringLiteral("SHA512");
    case OtpAccount::Algorithm::Sha1:
        break;
    }
    return QStringLiteral("SHA1");
}

}
