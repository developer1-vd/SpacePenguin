#include "logins.h"

#include "importer.h"
#include "secretstore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <dpapi.h>
#endif

namespace spacepenguin {
namespace {

constexpr int kOriginRole = Qt::UserRole;
constexpr int kUsernameRole = Qt::UserRole + 1;
constexpr int kPasswordRole = Qt::UserRole + 2;
constexpr int kNeverRole = Qt::UserRole + 3;

#ifdef Q_OS_WIN
// The file is encrypted for the signed-in Windows account; DPAPI keeps the key.
QByteArray protect(const QByteArray &data, bool encrypt)
{
    DATA_BLOB in{DWORD(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.data()))};
    DATA_BLOB out{0, nullptr};
    const BOOL ok = encrypt ? CryptProtectData(&in, L"SpacePenguin logins", nullptr, nullptr, nullptr,
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
const char *const kFileName = "logins.dat";
#else
const char *const kFileName = "logins.json";
#endif

QString jsString(const QString &text)
{
    // A JSON string is a JavaScript string literal.
    const QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

} // namespace

LoginStore::LoginStore(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_path(directory.isEmpty() ? QString() : QDir(directory).filePath(QString::fromLatin1(kFileName)))
{
    load();
}

QString LoginStore::originOf(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http"))
        return {};
    if (url.host().isEmpty())
        return {};
    return url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment
                        | QUrl::RemoveUserInfo)
        .toString();
}

QString LoginStore::storageDescription() const
{
#ifdef Q_OS_WIN
    return tr("Saved logins are encrypted with your Windows account: other accounts on this "
              "computer cannot read them.");
#else
    // In the keyring once a write there worked; the file is then gone.
    if (m_inKeyring && !m_readOnly && !QFile::exists(m_path))
        return tr("Saved logins are kept in %1, encrypted and unlocked with your login.")
            .arg(SecretStore::name());
    if (m_readOnly)
        return tr("Your keyring stayed locked, so saved logins can't be read. Logins saved now "
                  "last until SpacePenguin closes.");
    return tr("Saved logins are kept in a file that only your user account can read. No keyring "
              "could be used, so they are not encrypted: anyone who can sign in as you can see them.");
#endif
}

QList<Login> LoginStore::loginsFor(const QString &origin) const
{
    QList<Login> found;
    for (const Login &login : m_logins) {
        if (login.origin == origin)
            found.append(login);
    }
    // The one used last first.
    std::sort(found.begin(), found.end(),
              [](const Login &a, const Login &b) { return a.lastUsed > b.lastUsed; });
    return found;
}

bool LoginStore::contains(const QString &origin, const QString &username,
                          const QString &password) const
{
    for (const Login &login : m_logins) {
        if (login.origin == origin && login.username == username && login.password == password)
            return true;
    }
    return false;
}

void LoginStore::save(const QString &origin, const QString &username, const QString &password)
{
    if (origin.isEmpty() || password.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool replaced = false;
    for (Login &login : m_logins) {
        if (login.origin == origin && login.username == username) {
            login.password = password;
            login.lastUsed = now;
            replaced = true;
        }
    }
    if (!replaced)
        m_logins.append(Login{origin, username, password, now, now});
    m_never.remove(origin);
    store();
    emit changed();
}

int LoginStore::importLogins(const QList<Login> &logins)
{
    int count = 0;
    for (const Login &incoming : logins) {
        bool found = false;
        for (Login &login : m_logins) {
            if (login.origin == incoming.origin && login.username == incoming.username) {
                found = true;
                if (login.password != incoming.password) {
                    login.password = incoming.password;
                    ++count;
                }
            }
        }
        if (!found) {
            m_logins.append(incoming);
            ++count;
        }
    }
    if (count > 0) {
        store();
        emit changed();
    }
    return count;
}

void LoginStore::remove(const QString &origin, const QString &username)
{
    const qsizetype before = m_logins.size();
    m_logins.removeIf([&](const Login &login) {
        return login.origin == origin && login.username == username;
    });
    if (m_logins.size() != before) {
        store();
        emit changed();
    }
}

void LoginStore::markUsed(const QString &origin, const QString &username)
{
    for (Login &login : m_logins) {
        if (login.origin == origin && login.username == username)
            login.lastUsed = QDateTime::currentDateTimeUtc();
    }
    store();
}

void LoginStore::setNeverSave(const QString &origin, bool never)
{
    if (never == m_never.contains(origin))
        return;
    if (never)
        m_never.insert(origin);
    else
        m_never.remove(origin);
    store();
    emit changed();
}

QStringList LoginStore::neverSaved() const
{
    QStringList sites(m_never.cbegin(), m_never.cend());
    sites.sort();
    return sites;
}

QString LoginStore::keyringKey() const
{
    // One entry per profile: the file it would otherwise be kept in names it.
    return QStringLiteral("logins:") + m_path;
}

void LoginStore::load()
{
    if (m_path.isEmpty())
        return;
    QByteArray data;
    bool migrate = false;
#ifndef Q_OS_WIN
    // The keyring when there is one. A file from before (or from when there was no
    // keyring) moves into it, and the plain copy is deleted once it is safely there.
    switch (SecretStore::read(keyringKey(), &data)) {
    case SecretStore::Result::Found:
        m_inKeyring = true;
        break;
    case SecretStore::Result::NotFound:
        m_inKeyring = true;
        migrate = QFile::exists(m_path);
        break;
    case SecretStore::Result::Unavailable:
        // A keyring that is there but stayed locked must not be overwritten with
        // nothing: keep this session's changes in memory only.
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
    for (const QJsonValue &value : root.value(QStringLiteral("logins")).toArray()) {
        const QJsonObject entry = value.toObject();
        m_logins.append(Login{
            entry.value(QStringLiteral("origin")).toString(),
            entry.value(QStringLiteral("username")).toString(),
            entry.value(QStringLiteral("password")).toString(),
            QDateTime::fromString(entry.value(QStringLiteral("created")).toString(), Qt::ISODate),
            QDateTime::fromString(entry.value(QStringLiteral("lastUsed")).toString(), Qt::ISODate),
        });
    }
    for (const QJsonValue &value : root.value(QStringLiteral("never")).toArray())
        m_never.insert(value.toString());
    if (migrate)
        store();
}

void LoginStore::store() const
{
    if (m_path.isEmpty() || m_readOnly)
        return;
    QJsonArray logins;
    for (const Login &login : m_logins) {
        logins.append(QJsonObject{
            {QStringLiteral("origin"), login.origin},
            {QStringLiteral("username"), login.username},
            {QStringLiteral("password"), login.password},
            {QStringLiteral("created"), login.created.toString(Qt::ISODate)},
            {QStringLiteral("lastUsed"), login.lastUsed.toString(Qt::ISODate)},
        });
    }
    QJsonArray never;
    for (const QString &origin : neverSaved())
        never.append(origin);
    QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("logins"), logins},
                                                {QStringLiteral("never"), never}})
                          .toJson(QJsonDocument::Compact);
#ifdef Q_OS_WIN
    data = protect(data, true);
    if (data.isEmpty())
        return;
#else
    if (m_inKeyring) {
        if (SecretStore::write(keyringKey(), data, tr("SpacePenguin saved logins"))) {
            QFile::remove(m_path);
            return;
        }
        // The keyring refused: keep the logins in the file rather than lose them.
    }
#endif
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    // Readable by the user's account only, from the moment it is written.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(data);
    file.commit();
}

QString LoginStore::captureToken()
{
    static const QString token = QStringLiteral("spacepenguin-login-%1:")
                                     .arg(QUuid::createUuid().toString(QUuid::Id128));
    return token;
}

QString LoginStore::captureScript(const QString &token)
{
    // Reports a sign-in when a form with a filled password field is sent: on
    // submit, on a click on its button, or on Enter in the password field. Forms
    // with differing passwords (changing a password) are left alone.
    return QStringLiteral(R"JS((function () {
  const token = %1;
  let last = '';
  function capture(root) {
    root = root || document;
    const fields = Array.from(root.querySelectorAll('input[type=password]')).filter(f => f.value);
    if (!fields.length || fields.some(f => f.value !== fields[0].value)) return;
    const password = fields[0];
    let username = '';
    for (const input of (password.form || document).querySelectorAll('input')) {
      if (input === password) break;
      if (/^(text|email|tel|)$/i.test(input.getAttribute('type') || '') && input.value)
        username = input.value;
    }
    const report = JSON.stringify({ o: location.origin, u: username, p: password.value });
    if (report === last) return;
    last = report;
    console.log(token + report);
  }
  document.addEventListener('submit', e => capture(e.target), true);
  document.addEventListener('click', e => {
    const button = e.target.closest && e.target.closest('button, input[type=submit], [role=button]');
    if (button) capture(button.form || document);
  }, true);
  document.addEventListener('keydown', e => {
    if (e.key === 'Enter' && e.target && e.target.type === 'password') capture(e.target.form || document);
  }, true);
})();)JS")
        .arg(jsString(token));
}

QString LoginStore::fillScript(const QString &username, const QString &password)
{
    return QStringLiteral(R"JS((function (username, password) {
  const field = Array.from(document.querySelectorAll('input[type=password]'))
      .find(f => f.offsetParent !== null) || document.querySelector('input[type=password]');
  let user = null;
  for (const input of (field ? (field.form || document) : document).querySelectorAll('input')) {
    if (input === field) break;
    if (/^(text|email|tel|)$/i.test(input.getAttribute('type') || '') && input.offsetParent !== null)
      user = input;
  }
  // Set values the way typing does, so the page's own scripts notice them.
  const set = (input, value) => {
    Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set.call(input, value);
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  };
  if (user && username) set(user, username);
  if (field) set(field, password);
  return !!(field || user);
})(%1, %2);)JS")
        .arg(jsString(username), jsString(password));
}

}
