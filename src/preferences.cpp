#include "preferences.h"

#include "urlresolver.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace spacepenguin {
namespace {

const QString kHome = QStringLiteral("browser/home");
const QString kSearch = QStringLiteral("browser/search");
const QString kAddedEngines = QStringLiteral("browser/addedSearchEngines");
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
const QString kHttpsOnly = QStringLiteral("privacy/httpsOnly");
const QString kSpellCheck = QStringLiteral("general/spellCheck");
const QString kRoles = QStringLiteral("roles/roles");
const QString kBypassCache = QStringLiteral("roles/developer/bypassCache");
const QString kUserAgent = QStringLiteral("roles/developer/userAgent");
const QString kPointerLock = QStringLiteral("roles/gamer/allowPointerLock");
const QString kDevToolsDock = QStringLiteral("roles/developer/devToolsDock");
const QString kWebsiteLanguages = QStringLiteral("languages/websites");
const QString kSpellCheckLanguages = QStringLiteral("languages/spelling");
const QString kAppLanguage = QStringLiteral("languages/app");
const QString kTranslateService = QStringLiteral("translate/service");
const QString kTranslateTarget = QStringLiteral("translate/target");
const QString kOfferTranslation = QStringLiteral("translate/offer");
const QString kLibreTranslateServer = QStringLiteral("translate/libreTranslateServer");
const QString kLibreTranslateKey = QStringLiteral("translate/libreTranslateKey");
const QString kReadAloudRate = QStringLiteral("browsing/readAloudRate");
const QString kMouseGestures = QStringLiteral("browsing/mouseGestures");
const QString kGpuBoost = QStringLiteral("roles/gamer/gpuBoost");
const QString kUncapFrameRate = QStringLiteral("roles/gamer/uncapFrameRate");
const QString kFpsCounter = QStringLiteral("roles/gamer/fpsCounter");
const QString kPauseOthers = QStringLiteral("roles/gamer/pauseOthersInFullScreen");
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
    return builtInSearchEngines() + addedSearchEngines();
}

QList<SearchEngine> Preferences::addedSearchEngines()
{
    QList<SearchEngine> engines;
    for (const QString &entry : QSettings().value(kAddedEngines).toStringList()) {
        const QString name = entry.section(QLatin1Char('\t'), 0, 0);
        const QString searchTemplate = entry.section(QLatin1Char('\t'), 1);
        if (!name.isEmpty() && searchTemplate.contains(QLatin1String("{query}")))
            engines.append({name, searchTemplate});
    }
    return engines;
}

QString Preferences::normalizeSearchTemplate(QString searchTemplate)
{
    searchTemplate = searchTemplate.trimmed();
    searchTemplate.replace(QLatin1String("%s"), QLatin1String("{query}"));
    const QUrl url(searchTemplate);
    if (!searchTemplate.contains(QLatin1String("{query}")) || url.host().isEmpty()
        || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")))
        return QString();
    return searchTemplate;
}

bool Preferences::addSearchEngine(const QString &name, QString searchTemplate)
{
    searchTemplate = normalizeSearchTemplate(searchTemplate);
    const QString cleanName = name.simplified();
    if (searchTemplate.isEmpty() || cleanName.isEmpty())
        return false;
    QStringList entries;
    for (const SearchEngine &engine : addedSearchEngines()) {
        if (engine.searchTemplate != searchTemplate)
            entries.append(engine.name + QLatin1Char('\t') + engine.searchTemplate);
    }
    entries.append(cleanName + QLatin1Char('\t') + searchTemplate);
    write(kAddedEngines, entries);
    return true;
}

void Preferences::removeSearchEngine(const QString &searchTemplate)
{
    QStringList entries;
    for (const SearchEngine &engine : addedSearchEngines()) {
        if (engine.searchTemplate != searchTemplate)
            entries.append(engine.name + QLatin1Char('\t') + engine.searchTemplate);
    }
    write(kAddedEngines, entries);
    if (this->searchTemplate() == searchTemplate)
        setSearchTemplate(QString());
}

QList<SearchEngine> Preferences::builtInSearchEngines()
{
    return {
        {QStringLiteral("DuckDuckGo"), QStringLiteral("https://duckduckgo.com/?q={query}")},
        {QStringLiteral("Startpage"), QStringLiteral("https://www.startpage.com/sp/search?q={query}")},
        {QStringLiteral("Brave Search"), QStringLiteral("https://search.brave.com/search?q={query}")},
        {QStringLiteral("Ecosia"), QStringLiteral("https://www.ecosia.org/search?q={query}")},
        {QStringLiteral("Google"), QStringLiteral("https://www.google.com/search?q={query}")},
        {QStringLiteral("Bing"), QStringLiteral("https://www.bing.com/search?q={query}")},
        {QStringLiteral("Qwant"), QStringLiteral("https://www.qwant.com/?q={query}")},
        {QStringLiteral("Mojeek"), QStringLiteral("https://www.mojeek.com/search?q={query}")},
        {QStringLiteral("Kagi"), QStringLiteral("https://kagi.com/search?q={query}")},
        {QStringLiteral("Yahoo"), QStringLiteral("https://search.yahoo.com/search?p={query}")},
        {QStringLiteral("Yandex"), QStringLiteral("https://yandex.com/search/?text={query}")},
        {QStringLiteral("Wikipedia"), QStringLiteral("https://en.wikipedia.org/w/index.php?search={query}")},
        // Shops: everything typed in the address bar searches their products.
        {QStringLiteral("Amazon"), QStringLiteral("https://www.amazon.com/s?k={query}")},
        {QStringLiteral("eBay"), QStringLiteral("https://www.ebay.com/sch/i.html?_nkw={query}")},
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

bool Preferences::spellCheck() const
{
    return QSettings().value(kSpellCheck, true).toBool();
}

void Preferences::setSpellCheck(bool on)
{
    write(kSpellCheck, on);
}

bool Preferences::httpsOnly() const
{
    return QSettings().value(kHttpsOnly, false).toBool();
}

void Preferences::setHttpsOnly(bool on)
{
    write(kHttpsOnly, on);
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

QString Preferences::devToolsDock() const
{
    const QString dock = QSettings().value(kDevToolsDock, QStringLiteral("bottom")).toString();
    return dock == QLatin1String("right") || dock == QLatin1String("window") ? dock : QStringLiteral("bottom");
}

void Preferences::setDevToolsDock(const QString &dock)
{
    write(kDevToolsDock, dock);
}

QStringList Preferences::websiteLanguages() const
{
    return QSettings().value(kWebsiteLanguages).toStringList();
}

void Preferences::setWebsiteLanguages(const QStringList &tags)
{
    write(kWebsiteLanguages, tags);
}

QStringList Preferences::spellCheckLanguages() const
{
    return QSettings().value(kSpellCheckLanguages).toStringList();
}

void Preferences::setSpellCheckLanguages(const QStringList &dictionaries)
{
    write(kSpellCheckLanguages, dictionaries);
}

QString Preferences::appLanguage() const
{
    return QSettings().value(kAppLanguage).toString();
}

void Preferences::setAppLanguage(const QString &tag)
{
    write(kAppLanguage, tag);
}

QString Preferences::translateService() const
{
    return QSettings().value(kTranslateService, QStringLiteral("google")).toString();
}

void Preferences::setTranslateService(const QString &key)
{
    write(kTranslateService, key);
}

QString Preferences::translateTarget() const
{
    return QSettings().value(kTranslateTarget).toString();
}

void Preferences::setTranslateTarget(const QString &tag)
{
    write(kTranslateTarget, tag);
}

bool Preferences::offerTranslation() const
{
    return QSettings().value(kOfferTranslation, true).toBool();
}

void Preferences::setOfferTranslation(bool offer)
{
    write(kOfferTranslation, offer);
}

QString Preferences::libreTranslateServer() const
{
    return QSettings().value(kLibreTranslateServer).toString();
}

void Preferences::setLibreTranslateServer(const QString &url)
{
    write(kLibreTranslateServer, url.trimmed());
}

QString Preferences::libreTranslateKey() const
{
    return QSettings().value(kLibreTranslateKey).toString();
}

void Preferences::setLibreTranslateKey(const QString &key)
{
    write(kLibreTranslateKey, key.trimmed());
}

int Preferences::readAloudRate() const
{
    return qBound(-5, QSettings().value(kReadAloudRate, 0).toInt(), 5);
}

void Preferences::setReadAloudRate(int rate)
{
    write(kReadAloudRate, qBound(-5, rate, 5));
}

bool Preferences::mouseGestures() const
{
    return QSettings().value(kMouseGestures, true).toBool();
}

void Preferences::setMouseGestures(bool on)
{
    write(kMouseGestures, on);
}

bool Preferences::gpuBoost() const
{
    return QSettings().value(kGpuBoost, true).toBool();
}

void Preferences::setGpuBoost(bool boost)
{
    write(kGpuBoost, boost);
}

bool Preferences::uncapFrameRate() const
{
    return QSettings().value(kUncapFrameRate, false).toBool();
}

void Preferences::setUncapFrameRate(bool uncap)
{
    write(kUncapFrameRate, uncap);
}

bool Preferences::showFpsCounter() const
{
    return QSettings().value(kFpsCounter, false).toBool();
}

void Preferences::setShowFpsCounter(bool show)
{
    write(kFpsCounter, show);
}

bool Preferences::pauseOthersInFullScreen() const
{
    return QSettings().value(kPauseOthers, true).toBool();
}

void Preferences::setPauseOthersInFullScreen(bool pause)
{
    write(kPauseOthers, pause);
}

}
