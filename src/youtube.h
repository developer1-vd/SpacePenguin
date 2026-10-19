#pragma once

class QWebEngineProfile;
class QWebEngineView;

namespace spacepenguin {

/**
 * Ad and sponsor removal for YouTube in the desktop app, which request blocking
 * alone cannot do: YouTube serves its ads from the same hosts as the videos and
 * schedules them inside the player's own data.
 *
 * - The ad entries are removed from the player data as the page receives it, so
 *   the player has no ads to play (YouTubeScripts::adRemoval).
 * - Should one start anyway, it is muted, run to its end and skipped, and ad
 *   boxes are hidden (YouTubeScripts::player).
 * - Sponsor segments are skipped with SponsorBlock's community data.
 */
class YouTube
{
public:
    /** Installs or removes the page scripts on @p profile. */
    static void installScripts(QWebEngineProfile *profile, bool blockAds, bool skipSponsors);

    /** Delivers SponsorBlock segments to @p view's YouTube pages as they open. */
    static void attach(QWebEngineView *view);
};

}
