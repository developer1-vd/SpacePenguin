#include "logins.h"

#include "icons.h"
#include "importer.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

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

QString LoginStore::storageDescription()
{
#ifdef Q_OS_WIN
    return tr("Saved logins are encrypted with your Windows account: other accounts on this "
              "computer cannot read them.");
#else
    return tr("Saved logins are kept in a file that only your user account can read. They are "
              "not encrypted, so anyone who can sign in as you can see them.");
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

void LoginStore::load()
{
    if (m_path.isEmpty())
        return;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QByteArray data = file.readAll();
#ifdef Q_OS_WIN
    data = protect(data, false);
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
}

void LoginStore::store() const
{
    if (m_path.isEmpty())
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
    const report = JSON.stringify({ u: username, p: password.value });
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

// ---- LoginsDialog -------------------------------------------------------------

LoginsDialog::LoginsDialog(LoginStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
{
    setWindowTitle(tr("Saved Logins"));
    resize(640, 480);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search saved logins"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &LoginsDialog::rebuild);

    m_list = new QTreeWidget;
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Site"), tr("Username"), tr("Password")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto *show = new QPushButton(tr("Show password"));
    auto *copyUser = new QPushButton(tr("Copy username"));
    auto *copyPassword = new QPushButton(tr("Copy password"));
    auto *remove = new QPushButton(Icons::icon(Icons::Trash), tr("Delete"));
    const auto selected = [this]() -> QTreeWidgetItem * {
        const auto items = m_list->selectedItems();
        return items.isEmpty() ? nullptr : items.first();
    };
    const auto updateButtons = [=] {
        QTreeWidgetItem *item = selected();
        const bool login = item && !item->data(0, kNeverRole).toBool();
        show->setEnabled(login);
        copyUser->setEnabled(login);
        copyPassword->setEnabled(login);
        remove->setEnabled(item != nullptr);
    };
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, updateButtons);
    connect(show, &QPushButton::clicked, this, [selected] {
        if (QTreeWidgetItem *item = selected())
            item->setText(2, item->data(0, kPasswordRole).toString());
    });
    connect(copyUser, &QPushButton::clicked, this, [selected] {
        if (QTreeWidgetItem *item = selected())
            QApplication::clipboard()->setText(item->data(0, kUsernameRole).toString());
    });
    connect(copyPassword, &QPushButton::clicked, this, [selected] {
        if (QTreeWidgetItem *item = selected())
            QApplication::clipboard()->setText(item->data(0, kPasswordRole).toString());
    });
    connect(remove, &QPushButton::clicked, this, [this, selected] {
        QTreeWidgetItem *item = selected();
        if (!item)
            return;
        const QString origin = item->data(0, kOriginRole).toString();
        if (item->data(0, kNeverRole).toBool())
            m_store->setNeverSave(origin, false);
        else
            m_store->remove(origin, item->data(0, kUsernameRole).toString());
    });
    connect(m_store, &LoginStore::changed, this, &LoginsDialog::rebuild);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(show);
    buttons->addWidget(copyUser);
    buttons->addWidget(copyPassword);
    buttons->addStretch(1);
    auto *import = new QPushButton(tr("Import…"));
    import->setToolTip(tr("From a passwords CSV file exported by Chrome, Edge, Firefox or a password manager"));
    connect(import, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import logins"), QDir::homePath(),
                                                          tr("Passwords file (*.csv);;All files (*)"));
        if (path.isEmpty())
            return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return;
        const QList<Login> found = Importer::parseLoginsCsv(QString::fromUtf8(file.readAll()));
        const int added = m_store->importLogins(found);
        QMessageBox::information(
            this, tr("Import logins"),
            found.isEmpty() ? tr("No logins were found. The file should be a passwords CSV, with url, "
                                 "username and password columns.")
                            : tr("Imported %n login(s). Delete the CSV file now: it holds your passwords "
                                 "unprotected.", nullptr, added));
    });
    buttons->addWidget(import);
    buttons->addWidget(remove);

    auto *note = new QLabel(LoginStore::storageDescription());
    note->setWordWrap(true);
    note->setProperty("muted", true);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_search);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(note);
    layout->addWidget(close);

    rebuild();
    updateButtons();
}

void LoginsDialog::rebuild()
{
    m_list->clear();
    const QString filter = m_search->text().trimmed();
    const auto matches = [&filter](const QString &a, const QString &b) {
        return filter.isEmpty() || a.contains(filter, Qt::CaseInsensitive)
            || b.contains(filter, Qt::CaseInsensitive);
    };
    for (const Login &login : m_store->logins()) {
        if (!matches(login.origin, login.username))
            continue;
        auto *item = new QTreeWidgetItem(m_list, {login.origin, login.username,
                                                  QString(8, QChar(0x2022))});
        item->setData(0, kOriginRole, login.origin);
        item->setData(0, kUsernameRole, login.username);
        item->setData(0, kPasswordRole, login.password);
    }
    for (const QString &origin : m_store->neverSaved()) {
        if (!matches(origin, QString()))
            continue;
        auto *item = new QTreeWidgetItem(m_list, {origin, tr("Never saved"), QString()});
        item->setData(0, kOriginRole, origin);
        item->setData(0, kNeverRole, true);
        item->setForeground(1, palette().color(QPalette::PlaceholderText));
    }
    m_list->sortItems(0, Qt::AscendingOrder);
    if (m_list->topLevelItemCount() == 0) {
        auto *empty = new QTreeWidgetItem(m_list, {tr("No saved logins yet. When you sign in to "
                                                      "a site, SpacePenguin offers to save it.")});
        empty->setFlags(Qt::NoItemFlags);
        empty->setFirstColumnSpanned(true);
    }
}

}
