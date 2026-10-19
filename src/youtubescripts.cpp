#include "youtubescripts.h"

namespace spacepenguin::YouTubeScripts {
namespace {

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

} // namespace

QString adRemoval()
{
    return QString::fromLatin1(kAdRemovalScript);
}

QString player(bool blockAds, bool skipSponsors)
{
    QString script = QString::fromLatin1(kPlayerScript);
    script.replace(QStringLiteral("%BLOCK_ADS%"), blockAds ? QStringLiteral("true") : QStringLiteral("false"));
    script.replace(QStringLiteral("%SKIP_SPONSORS%"),
                   skipSponsors ? QStringLiteral("true") : QStringLiteral("false"));
    return script;
}

}
