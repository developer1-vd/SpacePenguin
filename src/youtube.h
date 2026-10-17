#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QByteArray;
class QUrl;
class QWebEngineProfile;
class QWebEngineView;

namespace spacepenguin {

/**
 * Ad and sponsor removal for YouTube, which request blocking alone cannot do:
 * YouTube serves its ads from the same hosts as the videos and schedules them
 * inside the player's own data.
 *
 * - The ad entries (adPlacements, playerAds, adSlots) are removed from the
 *   player data as the page receives it, so the player has no ads to play.
 * - Should one start anyway, it is muted, run to its end and skipped.
 * - Ad boxes in the feed, the sidebar and search results are hidden.
 * - Sponsor segments are skipped with SponsorBlock's community data
 *   (sponsor.ajay.app). Only the first four hex digits of the SHA-256 of the
 *   video's ID are sent, so the service cannot tell which video is playing.
 */
class YouTube
{
public:
    struct Segment {
        double start = 0;
        double end = 0;
        QString category;
    };

    /** Installs or removes the page scripts on @p profile. */
    static void installScripts(QWebEngineProfile *profile, bool blockAds, bool skipSponsors);

    /** Delivers SponsorBlock segments to @p view's YouTube pages as they open. */
    static void attach(QWebEngineView *view);

    /** The video a YouTube URL plays (watch, shorts, live, embed, youtu.be), or empty. */
    static QString videoId(const QUrl &url);

    /** What SponsorBlock is asked for: the first 4 hex digits of SHA-256(videoId). */
    static QString hashPrefix(const QString &videoId);

    /** The skippable segments of @p videoId in a SponsorBlock hash-prefix response. */
    static QList<Segment> parseSegments(const QByteArray &json, const QString &videoId);

    /** Categories that are skipped. */
    static QStringList categories();
};

}
