#include <QtTest>

#include "sponsorblock.h"

#include <QCryptographicHash>

using namespace spacepenguin;

class TestSponsorBlock : public QObject
{
    Q_OBJECT

private slots:
    void videoIdFromEveryUrlShape_data();
    void videoIdFromEveryUrlShape();
    void hashPrefixIsFourHexDigitsOfSha256();
    void parsesOnlyThisVideosSkippableSegments();
    void toleratesGarbage();
};

void TestSponsorBlock::videoIdFromEveryUrlShape_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("id");
    QTest::newRow("watch") << "https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42" << "dQw4w9WgXcQ";
    QTest::newRow("mobile") << "https://m.youtube.com/watch?v=dQw4w9WgXcQ" << "dQw4w9WgXcQ";
    QTest::newRow("shorts") << "https://www.youtube.com/shorts/abcDEF_-123" << "abcDEF_-123";
    QTest::newRow("live") << "https://www.youtube.com/live/abcDEF_-123?si=x" << "abcDEF_-123";
    QTest::newRow("embed") << "https://www.youtube-nocookie.com/embed/abcDEF_-123" << "abcDEF_-123";
    QTest::newRow("short link") << "https://youtu.be/dQw4w9WgXcQ?t=1" << "dQw4w9WgXcQ";
    QTest::newRow("home") << "https://www.youtube.com/" << "";
    QTest::newRow("channel") << "https://www.youtube.com/@someone/videos" << "";
    QTest::newRow("bad id") << "https://www.youtube.com/watch?v=short" << "";
    QTest::newRow("other site") << "https://example.com/watch?v=dQw4w9WgXcQ" << "";
    QTest::newRow("lookalike") << "https://notyoutube.com/watch?v=dQw4w9WgXcQ" << "";
}

void TestSponsorBlock::videoIdFromEveryUrlShape()
{
    QFETCH(QString, url);
    QFETCH(QString, id);
    QCOMPARE(SponsorBlock::videoId(QUrl(url)), id);
}

void TestSponsorBlock::hashPrefixIsFourHexDigitsOfSha256()
{
    const QString prefix = SponsorBlock::hashPrefix(QStringLiteral("dQw4w9WgXcQ"));
    QCOMPARE(prefix.size(), 4);
    const QByteArray full =
        QCryptographicHash::hash("dQw4w9WgXcQ", QCryptographicHash::Sha256).toHex();
    QVERIFY(full.startsWith(prefix.toLatin1()));
}

void TestSponsorBlock::parsesOnlyThisVideosSkippableSegments()
{
    // A hash-prefix response lists every video sharing the prefix.
    const QByteArray json = R"([
      {"videoID": "otherVideo1", "segments": [{"segment": [1, 50], "category": "sponsor", "actionType": "skip"}]},
      {"videoID": "dQw4w9WgXcQ", "segments": [
        {"segment": [120.5, 180], "category": "sponsor", "actionType": "skip"},
        {"segment": [10, 25], "category": "selfpromo", "actionType": "skip"},
        {"segment": [200, 230], "category": "sponsor", "actionType": "mute"},
        {"segment": [300, 310], "category": "intro", "actionType": "skip"},
        {"segment": [400, 400.4], "category": "sponsor", "actionType": "skip"},
        {"segment": [500, 520], "category": "interaction"}
      ]}
    ])";
    const QList<SponsorBlock::Segment> segments = SponsorBlock::parseSegments(json, QStringLiteral("dQw4w9WgXcQ"));
    QCOMPARE(segments.size(), 3);
    QCOMPARE(segments.at(0).start, 10.0); // sorted by start
    QCOMPARE(segments.at(0).category, QStringLiteral("selfpromo"));
    QCOMPARE(segments.at(1).start, 120.5);
    QCOMPARE(segments.at(1).end, 180.0);
    QCOMPARE(segments.at(2).category, QStringLiteral("interaction")); // actionType defaults to skip
}

void TestSponsorBlock::toleratesGarbage()
{
    QVERIFY(SponsorBlock::parseSegments("not json", QStringLiteral("dQw4w9WgXcQ")).isEmpty());
    QVERIFY(SponsorBlock::parseSegments("{}", QStringLiteral("dQw4w9WgXcQ")).isEmpty());
    QVERIFY(SponsorBlock::parseSegments(R"([{"videoID":"dQw4w9WgXcQ","segments":[{"segment":[1]}]}])",
                                   QStringLiteral("dQw4w9WgXcQ")).isEmpty());
}

QTEST_GUILESS_MAIN(TestSponsorBlock)
#include "tst_sponsorblock.moc"
