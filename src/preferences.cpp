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

bool Preferences::blockingEnabled() const
{
    return QSettings().value(kBlocking, true).toBool();
}

void Preferences::setBlockingEnabled(bool enabled)
{
    write(kBlocking, enabled);
}

}
