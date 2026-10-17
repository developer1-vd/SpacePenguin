#include "profiles.h"

#include "chromeextensions.h"
#include "logins.h"
#include "notifications.h"

#include "adblocker.h"
#include "bookmarkmanager.h"
#include "cookiemanager.h"
#include "cosmeticfilters.h"
#include "datasavermanager.h"
#include "downloadmanager.h"
#include "historymanager.h"
#include "preferences.h"
#include "startpageschemehandler.h"
#include "store.h"
#include "userextensions.h"
#include "youtube.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QWebEngineNotification>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace spacepenguin {
namespace {

QString cosmeticScriptName()
{
    return UserScripts::reservedPrefix() + QStringLiteral("cosmetic");
}

/**
 * Installs (or replaces) the element-hiding script derived from the filter list.
 * Network blocking cannot remove an ad that is served from the same host as the
 * page, so cosmetic rules are what actually clear inline ads and pop-ups.
 */
void installCosmeticFilter(QWebEngineProfile *profile, AdBlocker *blocker)
{
    if (!profile)
        return;

    QWebEngineScriptCollection *scripts = profile->scripts();
    const QString source = blocker ? buildCosmeticSource(blocker->cosmeticGroups())
                                   : QString();

    const QList<QWebEngineScript> existing = scripts->toList();
    for (const QWebEngineScript &script : existing) {
        if (script.name() == cosmeticScriptName())
            scripts->remove(script);
    }

    if (source.isEmpty())
        return;

    QWebEngineScript script;
    script.setName(cosmeticScriptName());
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setSourceCode(source);
    scripts->insert(script);
}

bool writeStarterFilterList(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile bundled(QStringLiteral(":/filters/default.txt"));
    if (!bundled.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray contents = bundled.readAll();
    bundled.close();

    QFile target(path);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    return target.write(contents) == contents.size();
}

} // namespace

QString defaultDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString extensionsDirectory()
{
    return defaultDataDirectory() + QStringLiteral("/extensions");
}

QString defaultFilterListPath()
{
    return defaultDataDirectory() + QStringLiteral("/adblock/filters.txt");
}

ProfileServices createProfileServices(QObject *parent, bool isPrivate, const QString &filterListPath)
{
    ProfileServices services;

    if (isPrivate) {
        services.profile = new QWebEngineProfile(parent);
    } else {
        const QString dataDirectory = defaultDataDirectory();
        QDir().mkpath(dataDirectory);
        services.profile = new QWebEngineProfile(QStringLiteral("default"), parent);
        services.profile->setPersistentStoragePath(dataDirectory);
        services.profile->setCachePath(dataDirectory + QStringLiteral("/cache"));
    }

    // Answers to "allow this site to …?" are kept; in private windows only until they close.
    services.profile->setPersistentPermissionsPolicy(
        isPrivate ? QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory
                  : QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    services.profile->setNotificationPresenter(
        [](std::unique_ptr<QWebEngineNotification> notification) {
            Notifications::instance()->present(std::move(notification));
        });

    // One store of saved logins; private windows can fill from it but do not add to it.
    static LoginStore *logins = new LoginStore(defaultDataDirectory(), qApp);
    services.logins = logins;
    if (!isPrivate) {
        QWebEngineScript capture;
        // Browser-owned: the extensions loader keeps scripts with this prefix.
        capture.setName(UserScripts::reservedPrefix() + QStringLiteral("login-capture"));
        capture.setSourceCode(LoginStore::captureScript(LoginStore::captureToken()));
        capture.setInjectionPoint(QWebEngineScript::DocumentReady);
        capture.setWorldId(QWebEngineScript::ApplicationWorld);
        capture.setRunsOnSubFrames(false);
        services.profile->scripts()->insert(capture);
    }

    services.adBlocker = new AdBlocker(parent);
    services.profile->setUrlRequestInterceptor(services.adBlocker);

    services.downloadManager = new DownloadManager(services.profile, parent);

    services.bookmarkManager = new BookmarkManager(parent);

    services.cookieManager = new CookieManager(services.profile, parent);

    services.dataSaverManager = new DataSaverManager(parent);

    services.historyManager = new HistoryManager(parent);

    auto *startPage = new StartPageSchemeHandler(isPrivate, services.profile);
    HistoryManager *history = services.historyManager;
    startPage->setTileProvider([history] {
        QList<StartPageTile> tiles;
        for (const HistoryEntry &entry : history->mostVisited(8))
            tiles.append(StartPageTile{entry.title, entry.url.toString()});
        return tiles;
    });
    services.profile->installUrlSchemeHandler(QByteArrayLiteral("sp"), startPage);

    const bool explicitList = !filterListPath.isEmpty();
    const QString listPath = explicitList ? filterListPath : defaultFilterListPath();
    if (!explicitList && !QFile::exists(listPath))
        writeStarterFilterList(listPath);

    if (!listPath.isEmpty())
        services.adBlocker->loadFilters(listPath);

    QWebEngineProfile *profile = services.profile;
    AdBlocker *blocker = services.adBlocker;
    QObject::connect(blocker, &AdBlocker::filtersChanged, blocker,
                     [profile, blocker] { installCosmeticFilter(profile, blocker); });
    installCosmeticFilter(profile, blocker);

    // Preferences that live on the services rather than on a window.
    Preferences *preferences = Preferences::instance();
    DownloadManager *downloads = services.downloadManager;
    const auto applyPreferences = [preferences, blocker, downloads] {
        if (blocker->isEnabled() != preferences->blockingEnabled())
            blocker->setEnabled(preferences->blockingEnabled());
        downloads->setDownloadPath(preferences->downloadDirectory());
    };
    QObject::connect(preferences, &Preferences::changed, blocker, applyPreferences);
    applyPreferences();

    // YouTube's ads and sponsor segments need page scripts, not request rules.
    const auto installYouTube = [profile, blocker, preferences] {
        const bool blockAds = blocker->isEnabled()
            && !blocker->isPausedFor(QStringLiteral("www.youtube.com"));
        YouTube::installScripts(profile, blockAds, preferences->skipSponsors());
    };
    QObject::connect(blocker, &AdBlocker::enabledChanged, blocker, installYouTube);
    QObject::connect(blocker, &AdBlocker::pausedHostsChanged, blocker, installYouTube);
    QObject::connect(preferences, &Preferences::changed, blocker, installYouTube);
    installYouTube();

    // QtWebEngine runs Chrome extensions only in profiles kept on disk.
    if (!isPrivate && ChromeExtensions::isSupported())
        services.extensions = new ChromeExtensions(services.profile, parent);

    services.userScripts = new UserScripts(services.profile, parent);
    startPage->setStore(new Store(services.userScripts, parent));
    if (!isPrivate)
        services.userScripts->setDirectory(extensionsDirectory());

    return services;
}

}