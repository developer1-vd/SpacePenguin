#include "sponsorblock.h"

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

#include <algorithm>

namespace spacepenguin {
namespace {

const char kApi[] = "https://sponsor.ajay.app/api/skipSegments/";

QNetworkAccessManager *network()
{
    static auto *manager = new QNetworkAccessManager;
    return manager;
}

QHash<QString, QList<SponsorBlock::Segment>> &cache()
{
    static QHash<QString, QList<SponsorBlock::Segment>> segments;
    return segments;
}

} // namespace

QStringList SponsorBlock::categories()
{
    return {QStringLiteral("sponsor"), QStringLiteral("selfpromo"), QStringLiteral("interaction")};
}

QString SponsorBlock::videoId(const QUrl &url)
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

QString SponsorBlock::hashPrefix(const QString &videoId)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(videoId.toUtf8(), QCryptographicHash::Sha256).toHex().left(4));
}

QList<SponsorBlock::Segment> SponsorBlock::parseSegments(const QByteArray &json, const QString &videoId)
{
    QList<Segment> segments;
    const QStringList wanted = SponsorBlock::categories();
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

void SponsorBlock::lookup(const QString &videoId, QObject *context,
                          std::function<void(const QList<Segment> &)> done)
{
    if (videoId.isEmpty())
        return;
    if (cache().contains(videoId)) {
        done(cache().value(videoId));
        return;
    }

    QUrl url(QString::fromLatin1(kApi) + hashPrefix(videoId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("categories"),
                       QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(categories()))
                                             .toJson(QJsonDocument::Compact)));
    query.addQueryItem(QStringLiteral("actionTypes"), QStringLiteral("[\"skip\"]"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    QNetworkReply *reply = network()->get(request);
    QPointer<QObject> target(context);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, target, videoId, done] {
        reply->deleteLater();
        // 404 means no segments for any video with this prefix.
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 404)
            return; // try again next time the video opens
        const QList<Segment> segments =
            status == 404 ? QList<Segment>() : parseSegments(reply->readAll(), videoId);
        cache().insert(videoId, segments);
        if (target)
            done(segments);
    });
}

QString SponsorBlock::toJavaScript(const QList<Segment> &segments)
{
    QJsonArray list;
    for (const Segment &segment : segments) {
        list.append(QJsonObject{{QStringLiteral("start"), segment.start},
                                {QStringLiteral("end"), segment.end},
                                {QStringLiteral("category"), segment.category}});
    }
    return QString::fromUtf8(QJsonDocument(list).toJson(QJsonDocument::Compact));
}

}
