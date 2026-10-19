#include "youtube.h"

#include "preferences.h"
#include "sponsorblock.h"
#include "userextensions.h"
#include "youtubescripts.h"

#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>

namespace spacepenguin {
namespace {

QString scriptName(const char *part)
{
    return UserScripts::reservedPrefix() + QStringLiteral("youtube-") + QLatin1String(part);
}

const QString kMatch = QStringLiteral(
    "// ==UserScript==\n"
    "// @match https://www.youtube.com/*\n"
    "// @match https://m.youtube.com/*\n"
    "// @match https://music.youtube.com/*\n"
    "// @match https://www.youtube-nocookie.com/*\n"
    "// ==/UserScript==\n");

void removeScripts(QWebEngineScriptCollection *scripts)
{
    const QList<QWebEngineScript> existing = scripts->toList();
    for (const QWebEngineScript &script : existing) {
        if (script.name().startsWith(scriptName("")))
            scripts->remove(script);
    }
}

void fetchAndDeliver(QWebEngineView *view)
{
    if (!Preferences::instance()->skipSponsors())
        return;
    const QString videoId = SponsorBlock::videoId(view->url());
    SponsorBlock::lookup(videoId, view, [view, videoId](const QList<SponsorBlock::Segment> &segments) {
        if (SponsorBlock::videoId(view->url()) != videoId)
            return; // the tab has moved on to another video
        // videoId() only returns [A-Za-z0-9_-]{11}, so it can be quoted as it is.
        view->page()->runJavaScript(
            QStringLiteral("window.__spSponsor && window.__spSponsor.set('%1', %2);")
                .arg(videoId, SponsorBlock::toJavaScript(segments)),
            QWebEngineScript::ApplicationWorld);
    });
}

} // namespace

void YouTube::installScripts(QWebEngineProfile *profile, bool blockAds, bool skipSponsors)
{
    if (!profile)
        return;
    QWebEngineScriptCollection *scripts = profile->scripts();
    removeScripts(scripts);
    if (!blockAds && !skipSponsors)
        return;

    if (blockAds) {
        // Page world, before YouTube's scripts: only then does it see the player data.
        QWebEngineScript ads;
        ads.setName(scriptName("ads"));
        ads.setInjectionPoint(QWebEngineScript::DocumentCreation);
        ads.setWorldId(QWebEngineScript::MainWorld);
        ads.setRunsOnSubFrames(true);
        ads.setSourceCode(kMatch + YouTubeScripts::adRemoval());
        scripts->insert(ads);
    }

    // SpacePenguin's own world: it shares the page's DOM, not its scripts.
    QWebEngineScript player;
    player.setName(scriptName("player"));
    player.setInjectionPoint(QWebEngineScript::DocumentReady);
    player.setWorldId(QWebEngineScript::ApplicationWorld);
    player.setRunsOnSubFrames(true);
    player.setSourceCode(kMatch + YouTubeScripts::player(blockAds, skipSponsors));
    scripts->insert(player);
}

void YouTube::attach(QWebEngineView *view)
{
    // YouTube changes videos without loading a new page, so watch the address.
    QObject::connect(view, &QWebEngineView::urlChanged, view, [view] { fetchAndDeliver(view); });
    // A fresh page has a fresh script that has not seen the segments yet.
    QObject::connect(view, &QWebEngineView::loadFinished, view, [view] { fetchAndDeliver(view); });
}

}
