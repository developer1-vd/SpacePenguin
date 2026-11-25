#include "authenticator.h"

#include "secretstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <dpapi.h>
#endif

namespace spacepenguin {
namespace {

#ifdef Q_OS_WIN
// Encrypted for the signed-in Windows account, as saved logins are (logins.cpp).
QByteArray protect(const QByteArray &data, bool encrypt)
{
    DATA_BLOB in{DWORD(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.data()))};
    DATA_BLOB out{0, nullptr};
    const BOOL ok = encrypt ? CryptProtectData(&in, L"SpacePenguin authenticator", nullptr, nullptr, nullptr,
                                               CRYPTPROTECT_UI_FORBIDDEN, &out)
                            : CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                                 CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!ok)
        return {};
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}
const char *const kFileName = "authenticator.dat";
#else
const char *const kFileName = "authenticator.json";
#endif

QString typeName(OtpAccount::Type type)
{
    return type == OtpAccount::Type::Hotp ? QStringLiteral("hotp") : QStringLiteral("totp");
}

OtpAccount::Algorithm algorithmFrom(const QString &name)
{
    if (name == QLatin1String("SHA256"))
        return OtpAccount::Algorithm::Sha256;
    if (name == QLatin1String("SHA512"))
        return OtpAccount::Algorithm::Sha512;
    return OtpAccount::Algorithm::Sha1;
}

} // namespace

AuthenticatorStore::AuthenticatorStore(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_path(directory.isEmpty() ? QString() : QDir(directory).filePath(QString::fromLatin1(kFileName)))
{
    load();
}

OtpAccount AuthenticatorStore::account(const QString &id) const
{
    for (const OtpAccount &account : m_accounts) {
        if (account.id == id)
            return account;
    }
    return {};
}

QList<OtpAccount> AuthenticatorStore::accountsFor(const QUrl &url) const
{
    QList<OtpAccount> found;
    for (const OtpAccount &account : m_accounts) {
        if (Otp::matchesSite(account, url))
            found.append(account);
    }
    return found;
}

QString AuthenticatorStore::add(OtpAccount account)
{
    if (!account.isValid())
        return {};
    for (OtpAccount &existing : m_accounts) {
        if (existing.secret == account.secret) {
            account.id = existing.id;
            account.created = existing.created;
            if (account.site.isEmpty())
                account.site = existing.site;
            existing = account;
            store();
            emit changed();
            return account.id;
        }
    }
    account.id = QUuid::createUuid().toString(QUuid::Id128);
    account.created = QDateTime::currentDateTimeUtc();
    m_accounts.append(account);
    store();
    emit changed();
    return account.id;
}

void AuthenticatorStore::remove(const QString &id)
{
    if (m_accounts.removeIf([&](const OtpAccount &account) { return account.id == id; }) > 0) {
        store();
        emit changed();
    }
}

void AuthenticatorStore::rename(const QString &id, const QString &issuer, const QString &name)
{
    for (OtpAccount &account : m_accounts) {
        if (account.id == id) {
            account.issuer = issuer.trimmed();
            account.name = name.trimmed();
            store();
            emit changed();
            return;
        }
    }
}

QString AuthenticatorStore::takeCode(const QString &id, qint64 unixTime)
{
    for (OtpAccount &account : m_accounts) {
        if (account.id != id)
            continue;
        const QString code = Otp::code(account, unixTime);
        if (account.type == OtpAccount::Type::Hotp) {
            // Each HOTP code works once: the next one is a new counter.
            ++account.counter;
            store();
            emit changed();
        }
        return code;
    }
    return {};
}

QString AuthenticatorStore::fillScript(const QString &code)
{
    // Digits only, so it goes into the script as it is.
    static const QRegularExpression digits(QStringLiteral("^[0-9]{6,10}$"));
    if (!digits.match(code).hasMatch())
        return QStringLiteral("false;");
    return QStringLiteral(R"JS((function (code) {
  const visible = e => e.offsetParent !== null && !e.disabled && !e.readOnly;
  const inputs = Array.from(document.querySelectorAll('input')).filter(visible)
      .filter(e => /^(text|tel|number|password|)$/i.test(e.getAttribute('type') || ''));
  // Set values the way typing does, so the page's own scripts notice them.
  const set = (input, value) => {
    input.focus();
    Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set.call(input, value);
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };
  // A row of one-character boxes, one per digit.
  const singles = inputs.filter(e => e.maxLength === 1);
  if (singles.length >= code.length) {
    const row = singles.slice(0, code.length);
    if (row.every(e => e.form === row[0].form)) {
      row.forEach((input, i) => set(input, code[i]));
      return true;
    }
  }
  const words = /one.?time|otp|totp|2fa|mfa|two.?factor|second.?factor|verif|auth.*code|security.?code|code|token|pin/i;
  const describe = e => [e.name, e.id, e.placeholder, e.getAttribute('aria-label'), e.className,
                         e.labels && e.labels[0] ? e.labels[0].textContent : ''].join(' ');
  const notIt = /postal|zip|promo|coupon|discount|gift|voucher|captcha|country|area|phone|search|referr/i;
  const field = inputs.find(e => (e.getAttribute('autocomplete') || '').includes('one-time-code'))
      || inputs.find(e => e.type !== 'password' && words.test(describe(e)) && !notIt.test(describe(e)))
      || inputs.find(e => /numeric/i.test(e.inputMode || '') && (e.maxLength < 0 || e.maxLength >= code.length)
                          && (e.maxLength < 0 || e.maxLength <= 10));
  if (!field) return false;
  set(field, code);
  return true;
})('%1');)JS")
        .arg(code);
}

QString AuthenticatorStore::storageDescription() const
{
#ifdef Q_OS_WIN
    return tr("Your authenticator keys are encrypted with your Windows account: other accounts on this "
              "computer cannot read them.");
#else
    if (m_inKeyring && !m_readOnly && !QFile::exists(m_path))
        return tr("Your authenticator keys are kept in %1, encrypted and unlocked with your login.")
            .arg(SecretStore::name());
    if (m_readOnly)
        return tr("Your keyring stayed locked, so the authenticator's keys can't be read. Accounts added now "
                  "last until SpacePenguin closes.");
    return tr("Your authenticator keys are kept in a file that only your user account can read. No keyring "
              "could be used, so they are not encrypted: anyone who can sign in as you can see them.");
#endif
}

QString AuthenticatorStore::keyringKey() const
{
    return QStringLiteral("authenticator:") + m_path;
}

void AuthenticatorStore::load()
{
    if (m_path.isEmpty())
        return;
    QByteArray data;
    bool migrate = false;
#ifndef Q_OS_WIN
    switch (SecretStore::read(keyringKey(), &data)) {
    case SecretStore::Result::Found:
        m_inKeyring = true;
        break;
    case SecretStore::Result::NotFound:
        m_inKeyring = true;
        migrate = QFile::exists(m_path);
        break;
    case SecretStore::Result::Unavailable:
        // A locked keyring must not be overwritten with nothing.
        m_readOnly = SecretStore::isAvailable();
        break;
    }
    if (!m_inKeyring || migrate) {
#endif
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly))
            return;
        data = file.readAll();
#ifdef Q_OS_WIN
        data = protect(data, false);
#else
    }
#endif
    const QJsonObject root = QJsonDocument::fromJson(data).object();
    for (const QJsonValue &value : root.value(QStringLiteral("accounts")).toArray()) {
        const QJsonObject entry = value.toObject();
        OtpAccount account;
        account.id = entry.value(QStringLiteral("id")).toString();
        account.issuer = entry.value(QStringLiteral("issuer")).toString();
        account.name = entry.value(QStringLiteral("name")).toString();
        account.secret = Otp::base32Decode(entry.value(QStringLiteral("secret")).toString());
        account.type = entry.value(QStringLiteral("type")).toString() == QLatin1String("hotp") ? OtpAccount::Type::Hotp
                                                                                              : OtpAccount::Type::Totp;
        account.algorithm = algorithmFrom(entry.value(QStringLiteral("algorithm")).toString());
        account.digits = qBound(6, entry.value(QStringLiteral("digits")).toInt(6), 8);
        account.period = qBound(10, entry.value(QStringLiteral("period")).toInt(30), 300);
        account.counter = entry.value(QStringLiteral("counter")).toString().toULongLong();
        account.site = entry.value(QStringLiteral("site")).toString();
        account.created = QDateTime::fromString(entry.value(QStringLiteral("created")).toString(), Qt::ISODate);
        if (account.id.isEmpty())
            account.id = QUuid::createUuid().toString(QUuid::Id128);
        if (account.isValid())
            m_accounts.append(account);
    }
    if (migrate)
        store();
}

void AuthenticatorStore::store() const
{
    if (m_path.isEmpty() || m_readOnly)
        return;
    QJsonArray accounts;
    for (const OtpAccount &account : m_accounts) {
        accounts.append(QJsonObject{
            {QStringLiteral("id"), account.id},
            {QStringLiteral("issuer"), account.issuer},
            {QStringLiteral("name"), account.name},
            {QStringLiteral("secret"), Otp::base32Encode(account.secret)},
            {QStringLiteral("type"), typeName(account.type)},
            {QStringLiteral("algorithm"), Otp::algorithmName(account.algorithm)},
            {QStringLiteral("digits"), account.digits},
            {QStringLiteral("period"), account.period},
            // A string: JSON numbers lose 64-bit counters.
            {QStringLiteral("counter"), QString::number(account.counter)},
            {QStringLiteral("site"), account.site},
            {QStringLiteral("created"), account.created.toString(Qt::ISODate)},
        });
    }
    QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                                {QStringLiteral("accounts"), accounts}})
                          .toJson(QJsonDocument::Compact);
#ifdef Q_OS_WIN
    data = protect(data, true);
    if (data.isEmpty())
        return;
#else
    if (m_inKeyring) {
        if (SecretStore::write(keyringKey(), data, tr("SpacePenguin authenticator"))) {
            QFile::remove(m_path);
            return;
        }
        // The keyring refused: keep the keys in the file rather than lose them.
    }
#endif
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(data);
    file.commit();
}

}
