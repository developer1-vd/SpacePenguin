#include "cookiemanager.h"

#include "icons.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkCookie>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace spacepenguin {
namespace {

bool sameCookie(const QNetworkCookie &a, const QNetworkCookie &b)
{
    return a.name() == b.name() && a.domain() == b.domain() && a.path() == b.path();
}

constexpr int kSiteRole = Qt::UserRole;
constexpr int kIndexRole = Qt::UserRole + 1;

} // namespace

CookieManager::CookieManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
    if (!m_profile)
        return;

    m_cookieStore = m_profile->cookieStore();
    connect(m_cookieStore, &QWebEngineCookieStore::cookieAdded, this, &CookieManager::onCookieAdded);
    connect(m_cookieStore, &QWebEngineCookieStore::cookieRemoved, this, &CookieManager::onCookieRemoved);
    // Chromium reports the cookies it already has through cookieAdded.
    m_cookieStore->loadAllCookies();
}

CookieManager::~CookieManager() = default;

QList<Cookie> CookieManager::allCookies() const
{
    QList<Cookie> result;
    result.reserve(m_cookies.size());
    for (const QNetworkCookie &cookie : m_cookies)
        result.append(networkCookieToCookie(cookie));
    return result;
}

QList<Cookie> CookieManager::cookiesForUrl(const QUrl &url) const
{
    QList<Cookie> result;
    const QString host = url.host().toLower();
    for (const QNetworkCookie &cookie : m_cookies) {
        QString domain = cookie.domain().toLower();
        if (domain.startsWith(QLatin1Char('.')))
            domain.remove(0, 1);
        if (host == domain || host.endsWith(QLatin1Char('.') + domain))
            result.append(networkCookieToCookie(cookie));
    }
    return result;
}

QString CookieManager::siteOf(const QString &host)
{
    QString site = host.toLower();
    while (site.startsWith(QLatin1Char('.')))
        site.remove(0, 1);
    if (site.startsWith(QLatin1String("www.")))
        site.remove(0, 4);
    return site;
}

QString CookieManager::siteOf(const Cookie &cookie)
{
    return siteOf(cookie.domain.host());
}

void CookieManager::deleteCookie(const Cookie &cookie)
{
    if (!m_cookieStore)
        return;
    for (const QNetworkCookie &stored : std::as_const(m_cookies)) {
        if (QString::fromUtf8(stored.name()) == cookie.name && stored.path() == cookie.path
            && siteOf(stored.domain()) == siteOf(cookie) && stored.domain().toLower().endsWith(cookie.domain.host())) {
            m_cookieStore->deleteCookie(stored);
            return;
        }
    }
}

void CookieManager::deleteAllCookies()
{
    if (!m_cookieStore)
        return;
    m_cookieStore->deleteAllCookies();
    m_cookies.clear();
    emit cookiesCleared();
    emit changed();
}

void CookieManager::deleteSessionCookies()
{
    if (!m_cookieStore)
        return;
    m_cookieStore->deleteSessionCookies();
    emit cookiesCleared();
}

void CookieManager::deleteCookiesForUrl(const QUrl &url)
{
    if (!m_cookieStore)
        return;
    for (const Cookie &cookie : cookiesForUrl(url))
        deleteCookie(cookie);
}

void CookieManager::deleteCookiesForSite(const QString &site)
{
    if (!m_cookieStore)
        return;
    const QList<QNetworkCookie> cookies = m_cookies;
    for (const QNetworkCookie &cookie : cookies) {
        if (siteOf(cookie.domain()) == site)
            m_cookieStore->deleteCookie(cookie);
    }
}

void CookieManager::onCookieAdded(const QNetworkCookie &cookie)
{
    // A cookie set again replaces the old one.
    m_cookies.removeIf([&cookie](const QNetworkCookie &stored) { return sameCookie(stored, cookie); });
    m_cookies.append(cookie);
    emit cookieAdded(networkCookieToCookie(cookie));
    emit changed();
}

void CookieManager::onCookieRemoved(const QNetworkCookie &cookie)
{
    m_cookies.removeIf([&cookie](const QNetworkCookie &stored) { return sameCookie(stored, cookie); });
    emit cookieRemoved(networkCookieToCookie(cookie));
    emit changed();
}

Cookie CookieManager::networkCookieToCookie(const QNetworkCookie &cookie) const
{
    Cookie result;
    result.name = QString::fromUtf8(cookie.name());
    result.value = QString::fromUtf8(cookie.value());
    QString domain = cookie.domain();
    while (domain.startsWith(QLatin1Char('.')))
        domain.remove(0, 1);
    result.domain = QUrl(QStringLiteral("http://") + domain);
    result.path = cookie.path();
    result.expirationDate = cookie.expirationDate();
    result.secure = cookie.isSecure();
    result.httpOnly = cookie.isHttpOnly();
    result.session = cookie.isSessionCookie();
    return result;
}

// ---- CookiesDialog ----------------------------------------------------------------

CookiesDialog::CookiesDialog(CookieManager *cookies, QWidget *parent)
    : QDialog(parent)
    , m_cookies(cookies)
{
    setWindowTitle(tr("Cookies and Site Data"));
    resize(640, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(10);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search sites"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(Icons::Search), QLineEdit::LeadingPosition);
    layout->addWidget(m_search);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Site"), tr("Cookies")});
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    layout->addWidget(m_tree, 1);

    auto *note = new QLabel(tr("Removing a site's cookies signs you out of it. Only cookies are removed "
                               "here: other data a site stores (local storage) stays, as Qt offers no "
                               "way to remove it one site at a time."));
    note->setWordWrap(true);
    note->setProperty("muted", true);
    layout->addWidget(note);

    auto *buttons = new QHBoxLayout;
    m_remove = new QPushButton(Icons::icon(Icons::Trash), tr("Remove"));
    auto *removeAll = new QPushButton(tr("Remove All…"));
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addWidget(m_remove);
    buttons->addWidget(removeAll);
    buttons->addStretch(1);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &CookiesDialog::rebuild);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            [this] { m_remove->setEnabled(!m_tree->selectedItems().isEmpty()); });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        const auto items = m_tree->selectedItems();
        if (items.isEmpty() || !m_cookies)
            return;
        QTreeWidgetItem *item = items.first();
        const QString site = item->data(0, kSiteRole).toString();
        const int index = item->data(0, kIndexRole).toInt();
        if (index < 0) {
            m_cookies->deleteCookiesForSite(site);
            return;
        }
        const QList<Cookie> all = m_cookies->allCookies();
        if (index < all.size())
            m_cookies->deleteCookie(all.at(index));
    });
    connect(removeAll, &QPushButton::clicked, this, [this] {
        if (m_cookies
            && QMessageBox::question(this, tr("Remove all cookies"),
                                     tr("Remove every site's cookies? You will be signed out of most websites."))
                == QMessageBox::Yes) {
            m_cookies->deleteAllCookies();
        }
    });
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);

    // Cookies arrive one by one; the list is redrawn once they settle.
    auto *pending = new QTimer(this);
    pending->setSingleShot(true);
    pending->setInterval(150);
    connect(pending, &QTimer::timeout, this, &CookiesDialog::rebuild);
    if (m_cookies)
        connect(m_cookies, &CookieManager::changed, pending, qOverload<>(&QTimer::start));
    rebuild();
}

void CookiesDialog::rebuild()
{
    m_tree->setSortingEnabled(false);
    m_tree->clear();
    m_remove->setEnabled(false);
    if (!m_cookies)
        return;

    const QString search = m_search->text().trimmed();
    const QList<Cookie> all = m_cookies->allCookies();
    QHash<QString, QTreeWidgetItem *> sites;
    const QLocale locale;
    for (int i = 0; i < all.size(); ++i) {
        const Cookie &cookie = all.at(i);
        const QString site = CookieManager::siteOf(cookie);
        if (!search.isEmpty() && !site.contains(search, Qt::CaseInsensitive))
            continue;
        QTreeWidgetItem *&parent = sites[site];
        if (!parent) {
            parent = new QTreeWidgetItem(m_tree);
            parent->setIcon(0, Icons::icon(Icons::Globe));
            parent->setText(0, site);
            parent->setData(0, kSiteRole, site);
            parent->setData(0, kIndexRole, -1);
        }
        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, cookie.name);
        item->setText(1, cookie.session ? tr("until you close SpacePenguin")
                                        : tr("until %1").arg(locale.toString(cookie.expirationDate.date(),
                                                                             QLocale::ShortFormat)));
        item->setToolTip(0, tr("%1 on %2%3").arg(cookie.name, cookie.domain.host(), cookie.path));
        item->setData(0, kSiteRole, site);
        item->setData(0, kIndexRole, i);
    }
    for (QTreeWidgetItem *site : std::as_const(sites))
        site->setText(1, QString::number(site->childCount()));
    if (sites.isEmpty()) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, search.isEmpty() ? tr("No cookies.") : tr("No site matches “%1”.").arg(search));
        item->setFlags(Qt::NoItemFlags);
    }
    m_tree->setSortingEnabled(true);
}

}
