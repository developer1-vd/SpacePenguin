#include "profiles.h"

#include "adblocker.h"
#include "bookmarkmanager.h"
#include "cookiemanager.h"
#include "cosmeticfilters.h"
#include "datasavermanager.h"
#include "downloadmanager.h"
#include "historymanager.h"
#include "startpageschemehandler.h"
#include "userextensions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
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

    services.profile->installUrlSchemeHandler(QByteArrayLiteral("sp"),
                                             new StartPageSchemeHandler(services.profile));

    services.adBlocker = new AdBlocker(parent);
    services.profile->setUrlRequestInterceptor(services.adBlocker);

    services.downloadManager = new DownloadManager(services.profile, parent);

    services.bookmarkManager = new BookmarkManager(parent);

    services.cookieManager = new CookieManager(services.profile, parent);

    services.dataSaverManager = new DataSaverManager(parent);

    services.historyManager = new HistoryManager(parent);

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

    services.userScripts = new UserScripts(services.profile, parent);
    if (!isPrivate)
        services.userScripts->setDirectory(extensionsDirectory());

    return services;
}

}