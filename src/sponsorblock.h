#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

class QByteArray;
class QObject;
class QUrl;

namespace spacepenguin {

/**
 * Sponsor segments of YouTube videos from SponsorBlock's community data
 * (sponsor.ajay.app), for the desktop and the mobile apps alike.
 *
 * Only the first four hex digits of the SHA-256 of a video's ID are sent; the
 * answer lists every video sharing that prefix (dozens), and the one wanted is
 * picked out locally, so the service cannot tell which video is playing.
 */
class SponsorBlock
{
public:
    struct Segment {
        double start = 0;
        double end = 0;
        QString category;
    };

    /** The video a YouTube URL plays (watch, shorts, live, embed, youtu.be), or empty. */
    static QString videoId(const QUrl &url);

    /** What SponsorBlock is asked for: the first 4 hex digits of SHA-256(videoId). */
    static QString hashPrefix(const QString &videoId);

    /** The skippable segments of @p videoId in a SponsorBlock hash-prefix response. */
    static QList<Segment> parseSegments(const QByteArray &json, const QString &videoId);

    /** Categories that are skipped. */
    static QStringList categories();

    /**
     * Looks up @p videoId (from a cache after the first time) and calls @p done with
     * its segments, unless @p context has been destroyed by then. Network errors
     * call nothing, so the next lookup tries again.
     */
    static void lookup(const QString &videoId, QObject *context,
                       std::function<void(const QList<Segment> &)> done);

    /** The segments as a JavaScript array literal, for the page scripts. */
    static QString toJavaScript(const QList<Segment> &segments);
};

}
