#pragma once

#include <QString>

namespace spacepenguin {

/**
 * The page scripts behind YouTube ad blocking and sponsor skipping, shared by the
 * desktop app (installed into QtWebEngine) and the mobile apps (run through
 * QtWebView).
 */
namespace YouTubeScripts {

/**
 * Removes the ad entries from YouTube's player data as the page receives it. It
 * only works when it runs before YouTube's own scripts, at document creation,
 * which QtWebEngine can do and the mobile web views cannot.
 */
QString adRemoval();

/**
 * Hides ad boxes, runs any ad that plays to its end, and skips the sponsor
 * segments handed to it with window.__spSponsor.set(videoId, segments). Safe to
 * run more than once in a page.
 */
QString player(bool blockAds, bool skipSponsors);

}

}
