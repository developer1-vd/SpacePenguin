#include "youtube.h"

#include "preferences.h"
#include "userextensions.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>

namespace spacepenguin {
namespace {

const char kSponsorBlockApi[] = "https://sponsor.ajay.app/api/skipSegments/";

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

/**
 * Runs in the page's own world, before YouTube's scripts, so it sees the player
 * data as YouTube parses it: from the inline ytInitialPlayerResponse, from
 * JSON.parse and from fetch().json(). Ad entries are deleted there.
 */
const char kAdRemovalScript[] = R"JS(
(function () {
  'use strict';
  if (window.__spYouTubeAds) return;
  window.__spYouTubeAds = true;

  var AD_KEYS = ['adPlacements', 'adSlots', 'playerAds', 'adBreakHeartbeatParams'];
  var prune = function (data) {
    try {
      if (!data || typeof data !== 'object') return data;
      var targets = [data, data.playerResponse, data.response && data.response.playerResponse];
      if (Array.isArray(data)) {
        for (var a = 0; a < data.length; a++) {
          if (data[a] && typeof data[a] === 'object') targets.push(data[a].playerResponse);
        }
      }
      for (var t = 0; t < targets.length; t++) {
        var target = targets[t];
        if (!target || typeof target !== 'object') continue;
        for (var k = 0; k < AD_KEYS.length; k++) {
          if (AD_KEYS[k] in target) delete target[AD_KEYS[k]];
        }
      }
    } catch (e) {}
    return data;
  };

  var originalParse = JSON.parse;
  JSON.parse = function () {
    return prune(originalParse.apply(this, arguments));
  };

  if (window.Response && Response.prototype.json) {
    var originalJson = Response.prototype.json;
    Response.prototype.json = function () {
      return originalJson.apply(this, arguments).then(prune);
    };
  }

  // The first video's data arrives as a global assigned by an inline script.
  ['ytInitialPlayerResponse', 'playerResponse'].forEach(function (name) {
    var value;
    try {
      Object.defineProperty(window, name, {
        configurable: true,
        get: function () { return value; },
        set: function (next) { value = prune(next); }
      });
    } catch (e) {}
  });
})();
)JS";

/**
 * Runs in SpacePenguin's own world (it shares the page's DOM, not its scripts):
 * hides ad boxes, runs any ad that still plays to its end, and skips the
 * sponsor segments the browser hands it through window.__spSponsor.set().
 * The two placeholders are replaced with true or false.
 */
const char kPlayerScript[] = R"JS(
(function () {
  'use strict';
  if (window.__spYouTubePlayer) return;
  window.__spYouTubePlayer = true;
  var BLOCK_ADS = %BLOCK_ADS%;
  var SKIP_SPONSORS = %SKIP_SPONSORS%;

  if (BLOCK_ADS) {
    var css = [
      '#masthead-ad', '#player-ads', 'ytd-ad-slot-renderer', 'ytd-in-feed-ad-layout-renderer',
      'ytd-banner-promo-renderer', 'ytd-statement-banner-renderer', 'ytd-display-ad-renderer',
      'ytd-promoted-sparkles-web-renderer', 'ytd-promoted-video-renderer',
      'ytd-compact-promoted-video-renderer', 'ytd-companion-slot-renderer',
      'ytd-action-companion-ad-renderer', 'ytd-player-legacy-desktop-watch-ads-renderer',
      'ytd-merch-shelf-renderer', 'ytd-search-pyv-renderer', 'ytd-video-masthead-ad-v3-renderer',
      'ytd-rich-item-renderer:has(ytd-ad-slot-renderer)',
      'ytd-rich-section-renderer:has(ytd-statement-banner-renderer)',
      'ytd-engagement-panel-section-list-renderer[target-id="engagement-panel-ads"]',
      '.ytp-ad-overlay-container', '.ytp-ad-image-overlay', '.ytp-ad-text-overlay',
      'ytm-promoted-sparkles-web-renderer', 'ytm-companion-ad-renderer', 'ad-slot-renderer'
    ].join(',') + '{display:none!important}';
    var style = document.createElement('style');
    style.textContent = css;
    (document.head || document.documentElement).appendChild(style);
  }

  var segments = {};
  window.__spSponsor = {
    set: function (videoId, list) { segments[videoId] = list || []; }
  };

  var currentVideoId = function () {
    var match = location.pathname.match(/^\/(?:shorts|live|embed)\/([\w-]{11})/);
    if (match) return match[1];
    return new URLSearchParams(location.search).get('v') || '';
  };

  var LABELS = { sponsor: 'sponsor', selfpromo: 'self-promotion', interaction: 'subscribe reminder' };
  var toast = function (text) {
    var player = document.querySelector('.html5-video-player') || document.body;
    var note = document.createElement('div');
    note.textContent = text;
    note.style.cssText = 'position:absolute;left:12px;bottom:64px;z-index:9999;padding:6px 12px;' +
      'border-radius:14px;background:rgba(20,22,28,.85);color:#fff;font:500 13px sans-serif;' +
      'pointer-events:none;transition:opacity .4s';
    player.appendChild(note);
    setTimeout(function () { note.style.opacity = '0'; }, 2200);
    setTimeout(function () { note.remove(); }, 2700);
  };

  var mutedByUs = false;
  var lastSkip = null;
  setInterval(function () {
    var player = document.querySelector('.html5-video-player');
    var video = player && player.querySelector('video');
    if (!video) return;

    var adShowing = player.classList.contains('ad-showing') || player.classList.contains('ad-interrupting');
    if (BLOCK_ADS && adShowing) {
      // An ad got through: silence it, run it to its end, press Skip.
      if (!video.muted) { video.muted = true; mutedByUs = true; }
      if (isFinite(video.duration) && video.duration > 0 && video.currentTime < video.duration - 0.2) {
        video.currentTime = video.duration - 0.1;
      }
      var skip = player.querySelector('.ytp-ad-skip-button, .ytp-ad-skip-button-modern, .ytp-skip-ad-button, button[id^="skip-button"]');
      if (skip) skip.click();
      return;
    }
    if (mutedByUs) { video.muted = false; mutedByUs = false; }

    if (!SKIP_SPONSORS) return;
    var list = segments[currentVideoId()];
    if (!list || video.paused && video.currentTime === 0) return;
    var now = video.currentTime;
    for (var i = 0; i < list.length; i++) {
      var s = list[i];
      if (now >= s.start && now < s.end - 0.25) {
        var key = currentVideoId() + ':' + s.start;
        // Seeking back into a segment right after skipping it means the viewer wants it.
        if (lastSkip && lastSkip.key === key && Date.now() - lastSkip.at < 4000) return;
        video.currentTime = s.end;
        lastSkip = { key: key, at: Date.now() };
        toast('Skipped ' + (LABELS[s.category] || s.category));
        return;
      }
    }
  }, 250);
})();
)JS";

void removeScripts(QWebEngineScriptCollection *scripts)
{
    const QList<QWebEngineScript> existing = scripts->toList();
    for (const QWebEngineScript &script : existing) {
        if (script.name().startsWith(scriptName("")))
            scripts->remove(script);
    }
}

QNetworkAccessManager *network()
{
    static auto *manager = new QNetworkAccessManager;
    return manager;
}

QHash<QString, QList<YouTube::Segment>> &segmentCache()
{
    static QHash<QString, QList<YouTube::Segment>> cache;
    return cache;
}

void deliver(QWebEngineView *view, const QString &videoId, const QList<YouTube::Segment> &segments)
{
    QJsonArray list;
    for (const YouTube::Segment &segment : segments) {
        list.append(QJsonObject{{QStringLiteral("start"), segment.start},
                                {QStringLiteral("end"), segment.end},
                                {QStringLiteral("category"), segment.category}});
    }
    const QString arguments =
        QString::fromUtf8(QJsonDocument(QJsonArray{videoId, list}).toJson(QJsonDocument::Compact));
    const QString call =
        QStringLiteral("window.__spSponsor && window.__spSponsor.set.apply(null, %1);").arg(arguments);
    view->page()->runJavaScript(call, QWebEngineScript::ApplicationWorld);
}

void fetchAndDeliver(QWebEngineView *view)
{
    if (!Preferences::instance()->skipSponsors())
        return;
    const QString videoId = YouTube::videoId(view->url());
    if (videoId.isEmpty())
        return;

    if (segmentCache().contains(videoId)) {
        deliver(view, videoId, segmentCache().value(videoId));
        return;
    }

    QUrl url(QString::fromLatin1(kSponsorBlockApi) + YouTube::hashPrefix(videoId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("categories"),
                       QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(YouTube::categories()))
                                             .toJson(QJsonDocument::Compact)));
    query.addQueryItem(QStringLiteral("actionTypes"), QStringLiteral("[\"skip\"]"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    QNetworkReply *reply = network()->get(request);
    QPointer<QWebEngineView> target(view);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, target, videoId] {
        reply->deleteLater();
        // 404 means no segments for any video with this prefix.
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 404)
            return; // try again next time the video opens
        const QList<YouTube::Segment> segments =
            status == 404 ? QList<YouTube::Segment>() : YouTube::parseSegments(reply->readAll(), videoId);
        segmentCache().insert(videoId, segments);
        if (target && YouTube::videoId(target->url()) == videoId)
            deliver(target, videoId, segments);
    });
}

} // namespace

QStringList YouTube::categories()
{
    return {QStringLiteral("sponsor"), QStringLiteral("selfpromo"), QStringLiteral("interaction")};
}

void YouTube::installScripts(QWebEngineProfile *profile, bool blockAds, bool skipSponsors)
{
    if (!profile)
        return;
    QWebEngineScriptCollection *scripts = profile->scripts();
    removeScripts(scripts);
    if (!blockAds && !skipSponsors)
        return;

    if (blockAds) {
        QWebEngineScript ads;
        ads.setName(scriptName("ads"));
        ads.setInjectionPoint(QWebEngineScript::DocumentCreation);
        ads.setWorldId(QWebEngineScript::MainWorld);
        ads.setRunsOnSubFrames(true);
        ads.setSourceCode(kMatch + QString::fromLatin1(kAdRemovalScript));
        scripts->insert(ads);
    }

    QString player = QString::fromLatin1(kPlayerScript);
    player.replace(QStringLiteral("%BLOCK_ADS%"), blockAds ? QStringLiteral("true") : QStringLiteral("false"));
    player.replace(QStringLiteral("%SKIP_SPONSORS%"),
                   skipSponsors ? QStringLiteral("true") : QStringLiteral("false"));
    QWebEngineScript script;
    script.setName(scriptName("player"));
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(kMatch + player);
    scripts->insert(script);
}

void YouTube::attach(QWebEngineView *view)
{
    // YouTube changes videos without loading a new page, so watch the address.
    QObject::connect(view, &QWebEngineView::urlChanged, view, [view] { fetchAndDeliver(view); });
    // A fresh page has a fresh script that has not seen the segments yet.
    QObject::connect(view, &QWebEngineView::loadFinished, view, [view] { fetchAndDeliver(view); });
}

QString YouTube::videoId(const QUrl &url)
{
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{11}$"));
    const QString host = url.host().toLower();
    QString id;
    if (host == QLatin1String("youtu.be")) {
        id = url.path().mid(1).section(QLatin1Char('/'), 0, 0);
    } else if (host == QLatin1String("youtube.com") || host.endsWith(QLatin1String(".youtube.com"))
               || host == QLatin1String("www.youtube-nocookie.com")) {
        const QString path = url.path();
        if (path == QLatin1String("/watch"))
            id = QUrlQuery(url).queryItemValue(QStringLiteral("v"));
        else if (path.startsWith(QLatin1String("/shorts/")) || path.startsWith(QLatin1String("/live/"))
                 || path.startsWith(QLatin1String("/embed/")))
            id = path.section(QLatin1Char('/'), 2, 2);
    }
    return idPattern.match(id).hasMatch() ? id : QString();
}

QString YouTube::hashPrefix(const QString &videoId)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(videoId.toUtf8(), QCryptographicHash::Sha256).toHex().left(4));
}

QList<YouTube::Segment> YouTube::parseSegments(const QByteArray &json, const QString &videoId)
{
    QList<Segment> segments;
    const QStringList wanted = categories();
    const QJsonArray videos = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &video : videos) {
        const QJsonObject object = video.toObject();
        if (object.value(QStringLiteral("videoID")).toString() != videoId)
            continue;
        for (const QJsonValue &value : object.value(QStringLiteral("segments")).toArray()) {
            const QJsonObject entry = value.toObject();
            const QJsonArray range = entry.value(QStringLiteral("segment")).toArray();
            const QString category = entry.value(QStringLiteral("category")).toString();
            const QString action = entry.value(QStringLiteral("actionType")).toString(QStringLiteral("skip"));
            if (range.size() != 2 || action != QLatin1String("skip") || !wanted.contains(category))
                continue;
            Segment segment{range.at(0).toDouble(), range.at(1).toDouble(), category};
            if (segment.end - segment.start >= 1.0)
                segments.append(segment);
        }
    }
    std::sort(segments.begin(), segments.end(),
              [](const Segment &a, const Segment &b) { return a.start < b.start; });
    return segments;
}

}
