#include "browserbackend.h"

#include "preferences.h"
#include "sponsorblock.h"
#include "urlresolver.h"
#include "youtubescripts.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

namespace spacepenguin {
namespace {

const QString kBookmarks = QStringLiteral("mobile/bookmarks");
const QString kHistory = QStringLiteral("mobile/history");
constexpr int kHistoryLimit = 500;

bool isYouTube(const QUrl &url)
{
    const QString host = url.host().toLower();
    return host == QLatin1String("youtube.com") || host.endsWith(QLatin1String(".youtube.com"))
        || host == QLatin1String("www.youtube-nocookie.com");
}

bool hostMatches(const QString &host, const QString &domain)
{
    return domain.isEmpty() || host == domain || host.endsWith(QLatin1Char('.') + domain);
}

} // namespace

BrowserBackend::BrowserBackend(QObject *parent)
    : QObject(parent)
{
    connect(Preferences::instance(), &Preferences::changed, this, &BrowserBackend::settingsChanged);

    // The element-hiding rules of the starter list the desktop app ships, which
    // are all a page script can apply here.
    QFile list(QStringLiteral(":/filters/default.txt"));
    if (list.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!list.atEnd()) {
            const QString line = QString::fromUtf8(list.readLine()).trimmed();
            const qsizetype separator = line.indexOf(QLatin1String("##"));
            if (line.startsWith(QLatin1Char('!')) || separator < 0)
                continue;
            const QString selector = line.mid(separator + 2);
            if (selector.isEmpty() || selector.startsWith(QLatin1Char('+')))
                continue; // scriptlets are a different thing
            const QString domains = line.left(separator);
            for (const QString &domain : domains.split(QLatin1Char(','), Qt::KeepEmptyParts))
                m_cosmeticRules.append({domain.trimmed().toLower(), selector});
        }
    }
}

QUrl BrowserBackend::resolve(const QString &text) const
{
    const UrlResolver resolver(Preferences::instance()->searchTemplate());
    return resolver.resolve(text.trimmed());
}

QString BrowserBackend::displayUrl(const QUrl &url) const
{
    QString text = url.toDisplayString(QUrl::RemoveUserInfo);
    if (text.startsWith(QLatin1String("https://")))
        text = text.mid(8);
    if (text.startsWith(QLatin1String("www.")))
        text = text.mid(4);
    if (text.endsWith(QLatin1Char('/')) && text.count(QLatin1Char('/')) == 1)
        text.chop(1);
    return text;
}

QStringList BrowserBackend::searchEngines() const
{
    QStringList names;
    for (const SearchEngine &engine : Preferences::searchEngines())
        names << engine.name;
    return names;
}

int BrowserBackend::searchEngine() const
{
    const QList<SearchEngine> engines = Preferences::searchEngines();
    for (int i = 0; i < engines.size(); ++i) {
        if (engines.at(i).searchTemplate == Preferences::instance()->searchTemplate())
            return i;
    }
    return 0;
}

void BrowserBackend::setSearchEngine(int index)
{
    const QList<SearchEngine> engines = Preferences::searchEngines();
    if (index >= 0 && index < engines.size())
        Preferences::instance()->setSearchTemplate(engines.at(index).searchTemplate);
}

QString BrowserBackend::searchEngineName() const
{
    const QStringList names = searchEngines();
    return names.value(searchEngine());
}

bool BrowserBackend::blockAds() const
{
    return Preferences::instance()->blockingEnabled();
}

void BrowserBackend::setBlockAds(bool block)
{
    Preferences::instance()->setBlockingEnabled(block);
}

bool BrowserBackend::skipSponsors() const
{
    return Preferences::instance()->skipSponsors();
}

void BrowserBackend::setSkipSponsors(bool skip)
{
    Preferences::instance()->setSkipSponsors(skip);
}

QVariantList BrowserBackend::readList(const QString &key) const
{
    return QJsonDocument::fromJson(QSettings().value(key).toByteArray()).array().toVariantList();
}

void BrowserBackend::writeList(const QString &key, const QVariantList &list)
{
    QSettings().setValue(key, QJsonDocument(QJsonArray::fromVariantList(list)).toJson(QJsonDocument::Compact));
}

QVariantList BrowserBackend::bookmarks() const
{
    return readList(kBookmarks);
}

bool BrowserBackend::isBookmarked(const QUrl &url) const
{
    const QString target = url.toString();
    for (const QVariant &entry : bookmarks()) {
        if (entry.toMap().value(QStringLiteral("url")).toString() == target)
            return true;
    }
    return false;
}

void BrowserBackend::toggleBookmark(const QString &title, const QUrl &url)
{
    if (url.isEmpty())
        return;
    if (isBookmarked(url)) {
        removeBookmark(url);
        return;
    }
    QVariantList list = bookmarks();
    list.prepend(QVariantMap{{QStringLiteral("title"), title.isEmpty() ? displayUrl(url) : title},
                             {QStringLiteral("url"), url.toString()}});
    writeList(kBookmarks, list);
    emit bookmarksChanged();
}

void BrowserBackend::removeBookmark(const QUrl &url)
{
    QVariantList list = bookmarks();
    const QString target = url.toString();
    list.removeIf([&target](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("url")).toString() == target;
    });
    writeList(kBookmarks, list);
    emit bookmarksChanged();
}

QVariantList BrowserBackend::history() const
{
    return readList(kHistory);
}

void BrowserBackend::recordVisit(const QString &title, const QUrl &url)
{
    if (url.isEmpty() || !(url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")))
        return;
    QVariantList list = history();
    const QString target = url.toString();
    // One entry per address, newest first.
    list.removeIf([&target](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("url")).toString() == target;
    });
    list.prepend(QVariantMap{{QStringLiteral("title"), title.isEmpty() ? displayUrl(url) : title},
                             {QStringLiteral("url"), target},
                             {QStringLiteral("time"), QDateTime::currentSecsSinceEpoch()}});
    while (list.size() > kHistoryLimit)
        list.removeLast();
    writeList(kHistory, list);
    emit historyChanged();
}

void BrowserBackend::clearHistory()
{
    writeList(kHistory, {});
    emit historyChanged();
}

QString BrowserBackend::pageScript(const QUrl &url) const
{
    if (isYouTube(url))
        return blockAds() || skipSponsors() ? YouTubeScripts::player(blockAds(), skipSponsors()) : QString();
    if (!blockAds())
        return QString();

    const QString host = url.host().toLower();
    QStringList selectors;
    for (const auto &[domain, selector] : m_cosmeticRules) {
        if (hostMatches(host, domain))
            selectors << selector;
    }
    if (selectors.isEmpty())
        return QString();
    const QString css = selectors.join(QLatin1Char(',')) + QStringLiteral("{display:none!important}");
    const QString literal =
        QString::fromUtf8(QJsonDocument(QJsonArray{css}).toJson(QJsonDocument::Compact));
    return QStringLiteral("(function(){if(window.__spHide)return;window.__spHide=true;"
                          "var s=document.createElement('style');s.textContent=%1[0];"
                          "(document.head||document.documentElement).appendChild(s);})();")
        .arg(literal);
}

void BrowserBackend::lookUpSponsors(const QUrl &url)
{
    if (!skipSponsors())
        return;
    const QString videoId = SponsorBlock::videoId(url);
    SponsorBlock::lookup(videoId, this, [this, url, videoId](const QList<SponsorBlock::Segment> &segments) {
        // videoId() only returns [A-Za-z0-9_-]{11}, so it can be quoted as it is.
        emit sponsorScriptReady(url, QStringLiteral("window.__spSponsor && window.__spSponsor.set('%1', %2);")
                                         .arg(videoId, SponsorBlock::toJavaScript(segments)));
    });
}

QString BrowserBackend::version() const
{
    return QCoreApplication::applicationVersion();
}

}
