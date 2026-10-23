#include "preferences.h"

#include "urlresolver.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>

namespace spacepenguin {
namespace {

const QString kHome = QStringLiteral("browser/home");
const QString kSearch = QStringLiteral("browser/search");
const QString kRestore = QStringLiteral("browser/restoreSession");
const QString kHomeButton = QStringLiteral("appearance/showHomeButton");
const QString kBookmarksBar = QStringLiteral("appearance/showBookmarksBar");
const QString kMenuBar = QStringLiteral("appearance/showMenuBar");
const QString kSideTabs = QStringLiteral("appearance/sideTabs");
const QString kSideTabsCollapsed = QStringLiteral("appearance/sideTabsCollapsed");
const QString kSystemTitleBar = QStringLiteral("appearance/systemTitleBar");
const QString kDownloads = QStringLiteral("downloads/directory");
const QString kBlocking = QStringLiteral("adblock/enabled");
const QString kSponsors = QStringLiteral("adblock/skipSponsors");
const QString kStripTracking = QStringLiteral("adblock/stripTracking");
const QString kCheckUpdates = QStringLiteral("updates/check");
const QString kSaveLogins = QStringLiteral("privacy/offerToSaveLogins");
const QString kRoles = QStringLiteral("roles/roles");
const QString kBypassCache = QStringLiteral("roles/developer/bypassCache");
const QString kUserAgent = QStringLiteral("roles/developer/userAgent");
const QString kPointerLock = QStringLiteral("roles/gamer/allowPointerLock");
const QString kMuteOthers = QStringLiteral("roles/gamer/muteOthersInFullScreen");
const QString kFocusMode = QStringLiteral("roles/focus/on");
const QString kFocusSites = QStringLiteral("roles/focus/sites");
const QString kWorkPages = QStringLiteral("roles/worker/pages");
const QString kWorkPagesOnStart = QStringLiteral("roles/worker/openOnStart");
const QString kPageZoom = QStringLiteral("roles/teacher/pageZoom");

} // namespace

Preferences::Preferences(QObject *parent)
    : QObject(parent)
{
}

Preferences *Preferences::instance()
{
    static Preferences *preferences = new Preferences(QCoreApplication::instance());
    return preferences;
}

QList<SearchEngine> Preferences::searchEngines()
{
    return {
        {QStringLiteral("DuckDuckGo"), QStringLiteral("https://duckduckgo.com/?q={query}")},
        {QStringLiteral("Startpage"), QStringLiteral("https://www.startpage.com/sp/search?q={query}")},
        {QStringLiteral("Brave Search"), QStringLiteral("https://search.brave.com/search?q={query}")},
        {QStringLiteral("Ecosia"), QStringLiteral("https://www.ecosia.org/search?q={query}")},
        {QStringLiteral("Google"), QStringLiteral("https://www.google.com/search?q={query}")},
        {QStringLiteral("Bing"), QStringLiteral("https://www.bing.com/search?q={query}")},
    };
}

void Preferences::write(const QString &key, const QVariant &value)
{
    QSettings settings;
    if (settings.value(key) == value)
        return;
    settings.setValue(key, value);
    emit changed();
}

void Preferences::notifyChanged()
{
    emit changed();
}

QString Preferences::homePage() const
{
    const QString value = QSettings().value(kHome).toString().trimmed();
    return value.isEmpty() ? UrlResolver::defaultHomeUrl() : value;
}

void Preferences::setHomePage(const QString &url)
{
    write(kHome, url.trimmed().isEmpty() ? UrlResolver::defaultHomeUrl() : url.trimmed());
}

QString Preferences::searchTemplate() const
{
    const QString value = QSettings().value(kSearch).toString().trimmed();
    if (value.isEmpty() || !value.contains(QLatin1String("{query}")))
        return UrlResolver::defaultSearchTemplate();
    return value;
}

void Preferences::setSearchTemplate(const QString &searchTemplate)
{
    write(kSearch, searchTemplate.trimmed());
}

bool Preferences::restoreSession() const
{
    return QSettings().value(kRestore, true).toBool();
}

void Preferences::setRestoreSession(bool restore)
{
    write(kRestore, restore);
}

bool Preferences::showHomeButton() const
{
    return QSettings().value(kHomeButton, true).toBool();
}

void Preferences::setShowHomeButton(bool show)
{
    write(kHomeButton, show);
}

bool Preferences::showBookmarksBar() const
{
    return QSettings().value(kBookmarksBar, true).toBool();
}

void Preferences::setShowBookmarksBar(bool show)
{
    write(kBookmarksBar, show);
}

bool Preferences::showMenuBar() const
{
    return QSettings().value(kMenuBar, false).toBool();
}

void Preferences::setShowMenuBar(bool show)
{
    write(kMenuBar, show);
}

bool Preferences::offerToSaveLogins() const
{
    return QSettings().value(kSaveLogins, true).toBool();
}

void Preferences::setOfferToSaveLogins(bool offer)
{
    write(kSaveLogins, offer);
}

bool Preferences::sideTabs() const
{
    return QSettings().value(kSideTabs, false).toBool();
}

void Preferences::setSideTabs(bool side)
{
    write(kSideTabs, side);
}

bool Preferences::sideTabsCollapsed() const
{
    return QSettings().value(kSideTabsCollapsed, false).toBool();
}

void Preferences::setSideTabsCollapsed(bool collapsed)
{
    write(kSideTabsCollapsed, collapsed);
}

bool Preferences::useSystemTitleBar() const
{
    return QSettings().value(kSystemTitleBar, false).toBool();
}

void Preferences::setUseSystemTitleBar(bool use)
{
    write(kSystemTitleBar, use);
}

QString Preferences::downloadDirectory() const
{
    const QString value = QSettings().value(kDownloads).toString();
    if (!value.isEmpty())
        return value;
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

void Preferences::setDownloadDirectory(const QString &path)
{
    write(kDownloads, path);
}

bool Preferences::skipSponsors() const
{
    return QSettings().value(kSponsors, true).toBool();
}

void Preferences::setSkipSponsors(bool skip)
{
    write(kSponsors, skip);
}

bool Preferences::checkForUpdates() const
{
    return QSettings().value(kCheckUpdates, false).toBool();
}

void Preferences::setCheckForUpdates(bool check)
{
    write(kCheckUpdates, check);
}

bool Preferences::stripTracking() const
{
    return QSettings().value(kStripTracking, true).toBool();
}

void Preferences::setStripTracking(bool strip)
{
    write(kStripTracking, strip);
}

bool Preferences::blockingEnabled() const
{
    return QSettings().value(kBlocking, true).toBool();
}

void Preferences::setBlockingEnabled(bool enabled)
{
    write(kBlocking, enabled);
}

QStringList Preferences::roles() const
{
    return QSettings().value(kRoles).toStringList();
}

void Preferences::setRoles(const QStringList &roles)
{
    write(kRoles, roles);
}

bool Preferences::hasRole(const QString &role) const
{
    return roles().contains(role);
}

bool Preferences::bypassCache() const
{
    return QSettings().value(kBypassCache, false).toBool();
}

void Preferences::setBypassCache(bool bypass)
{
    write(kBypassCache, bypass);
}

QString Preferences::userAgent() const
{
    return QSettings().value(kUserAgent).toString();
}

void Preferences::setUserAgent(const QString &userAgent)
{
    write(kUserAgent, userAgent.trimmed());
}

bool Preferences::allowPointerLock() const
{
    return QSettings().value(kPointerLock, true).toBool();
}

void Preferences::setAllowPointerLock(bool allow)
{
    write(kPointerLock, allow);
}

bool Preferences::muteOthersInFullScreen() const
{
    return QSettings().value(kMuteOthers, true).toBool();
}

void Preferences::setMuteOthersInFullScreen(bool mute)
{
    write(kMuteOthers, mute);
}

bool Preferences::focusMode() const
{
    return QSettings().value(kFocusMode, false).toBool();
}

void Preferences::setFocusMode(bool on)
{
    write(kFocusMode, on);
}

QStringList Preferences::focusSites() const
{
    static const QStringList defaults = {
        QStringLiteral("youtube.com"), QStringLiteral("tiktok.com"),   QStringLiteral("instagram.com"),
        QStringLiteral("facebook.com"), QStringLiteral("x.com"),       QStringLiteral("twitter.com"),
        QStringLiteral("reddit.com"),   QStringLiteral("twitch.tv"),   QStringLiteral("netflix.com"),
    };
    return QSettings().value(kFocusSites, defaults).toStringList();
}

void Preferences::setFocusSites(const QStringList &sites)
{
    write(kFocusSites, sites);
}

QStringList Preferences::workPages() const
{
    return QSettings().value(kWorkPages).toStringList();
}

void Preferences::setWorkPages(const QStringList &pages)
{
    write(kWorkPages, pages);
}

bool Preferences::openWorkPagesOnStart() const
{
    return QSettings().value(kWorkPagesOnStart, false).toBool();
}

void Preferences::setOpenWorkPagesOnStart(bool open)
{
    write(kWorkPagesOnStart, open);
}

int Preferences::pageZoom() const
{
    return qBound(50, QSettings().value(kPageZoom, 100).toInt(), 300);
}

void Preferences::setPageZoom(int percent)
{
    write(kPageZoom, qBound(50, percent, 300));
}

}
