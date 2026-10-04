#include <QtTest>

#include <memory>

#include "adblocker.h"
#include "cosmeticfilters.h"

using namespace spacepenguin;

class TestAdBlocker : public QObject
{
    Q_OBJECT

private slots:
    void emptyListBlocksNothing();
    void domainAnchoredRule();
    void plainSubstringRule();
    void wildcardRule();
    void caretSeparator();
    void exceptionsWin();
    void commentsAndHeadersIgnored();
    void invalidListRejected();
    void emptyFileRejected();
    void pauseForHost();
    void pauseCoversSubdomains();
    void hostCaseAndPortIgnored();
    void enableToggle();
    void countsRulesAndExceptions();

    void optionSuffixIsNotPartOfThePattern();
    void domainOptionLimitsRule();
    void negatedDomainOptionExcludes();
    void domainOptionRequiresDocument();
    void typeOptionSelectsResourceType();
    void generichideExceptionDoesNotBlockNetwork();
    void matchCaseOptionIsHonoured();
    void unknownOptionFallsBackToLiteralPattern();
    void generatedPatternsMatchExpectedTargets();
    void separatorBeforeOptions();
    void endOfAddressQuestionMark();
    void startAnchorMatchesScheme();

    void cosmeticRulesAreParsed();
    void cosmeticDomainScoping();
    void cosmeticExceptionsWin();
    void cosmeticGroupsCarryDomainScope();
    void cosmeticScriptHidesWithDomainScope();
    void cosmeticScriptIsEmptyWithoutRules();

    void youtubeInlineAdRequestsAreBlocked_data();
    void youtubeInlineAdRequestsAreBlocked();
    void youtubeCosmeticRulesHideOverlay();
    void popupAdsAreBlockedAsDocuments();
    void sameHostContentIsNotBlocked();
};

static std::unique_ptr<AdBlocker> blockerWith(const char *rules)
{
    auto blocker = std::make_unique<AdBlocker>();
    blocker->loadFiltersFromText(QString::fromLatin1(rules));
    return blocker;
}

static bool blocks(AdBlocker &blocker, const char *url,
                   RequestType type = RequestType::Any,
                   const QString &documentHost = QString())
{
    return blocker.wouldBlock(QUrl(QString::fromLatin1(url)), type, documentHost);
}

void TestAdBlocker::emptyListBlocksNothing()
{
    AdBlocker blocker;
    QVERIFY(!blocker.wouldBlock(QUrl(QStringLiteral("https://ads.example.com/x.js"))));
    QCOMPARE(blocker.ruleCount(), 0);
}

void TestAdBlocker::domainAnchoredRule()
{
    auto blocker = blockerWith("||ads.example.com^");

    QVERIFY(blocks(*blocker, "https://ads.example.com/banner.js"));
    QVERIFY(blocks(*blocker, "https://cdn.ads.example.com/x.js"));
    QVERIFY(!blocks(*blocker, "https://example.com/"));
    QVERIFY(!blocks(*blocker, "https://notads.example.com/"));
    QVERIFY(!blocks(*blocker, "https://example.com/ads.example.com"));
}

void TestAdBlocker::plainSubstringRule()
{
    auto blocker = blockerWith("tracker.example");

    QVERIFY(blocks(*blocker, "https://tracker.example/pixel.gif"));
    QVERIFY(blocks(*blocker, "https://a.tracker.example/pixel.gif"));
    QVERIFY(!blocks(*blocker, "https://example.com/pixel.gif"));
}

void TestAdBlocker::wildcardRule()
{
    auto blocker = blockerWith("||ads.*.cdn^");

    QVERIFY(blocks(*blocker, "https://ads.eu.cdn/x"));
    QVERIFY(blocks(*blocker, "https://x.ads.eu.cdn/x"));
    QVERIFY(!blocks(*blocker, "https://ads.cdn/x"));
    QVERIFY(!blocks(*blocker, "https://cdn/x"));
}

void TestAdBlocker::caretSeparator()
{
    auto blocker = blockerWith("||ads.example.com^");

    QVERIFY(!blocks(*blocker, "https://ads.example.computer/x"));
    QVERIFY(!blocks(*blocker, "https://ads.example.computer"));
    QVERIFY(blocks(*blocker, "https://ads.example.com/x"));
    QVERIFY(blocks(*blocker, "https://ads.example.com:443/x"));
}

void TestAdBlocker::exceptionsWin()
{
    auto blocker = blockerWith("||ads.example.com^\n@@||safe.ads.example.com^");

    QVERIFY(blocks(*blocker, "https://ads.example.com/x"));
    QVERIFY(!blocks(*blocker, "https://safe.ads.example.com/x"));
}

void TestAdBlocker::commentsAndHeadersIgnored()
{
    auto blocker =
        blockerWith("! Title: sample\n[Adblock Plus 2.0]\n# a comment\n||ads.example.com^\n");

    QCOMPARE(blocker->ruleCount(), 1);
    QCOMPARE(blocker->exceptionCount(), 0);
    QVERIFY(blocks(*blocker, "https://ads.example.com/x"));
}

void TestAdBlocker::invalidListRejected()
{
    AdBlocker blocker;
    QString error;
    QVERIFY(!blocker.loadFilters(QStringLiteral("/nonexistent/filters.txt"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!blocker.lastListError().isEmpty());
}

void TestAdBlocker::emptyFileRejected()
{
    AdBlocker blocker;
    QVERIFY(!blocker.loadFiltersFromText(QStringLiteral("! nothing but comments\n")));
    QCOMPARE(blocker.ruleCount(), 0);
}

void TestAdBlocker::pauseForHost()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("||ads.example.com^"));
    blocker.setPausedHosts({QStringLiteral("ADS.example.com")});

    QVERIFY(blocker.isPausedFor(QStringLiteral("ads.example.com")));
    QVERIFY(!blocks(blocker, "https://ads.example.com/x"));
    QVERIFY(!blocker.isPausedFor(QStringLiteral("other.com")));
}

void TestAdBlocker::pauseCoversSubdomains()
{
    AdBlocker blocker;
    blocker.setPausedHosts({QStringLiteral("example.com")});

    QVERIFY(blocker.isPausedFor(QStringLiteral("example.com")));
    QVERIFY(blocker.isPausedFor(QStringLiteral("cdn.example.com")));
    QVERIFY(!blocker.isPausedFor(QStringLiteral("notexample.com")));
}

void TestAdBlocker::hostCaseAndPortIgnored()
{
    auto blocker = blockerWith("||ads.example.com^");

    QVERIFY(blocks(*blocker, "https://ADS.Example.COM/banner.js"));
    QVERIFY(blocks(*blocker, "https://ads.example.com:8443/banner.js"));
    QVERIFY(!blocks(*blocker, "sp://start/"));
}

void TestAdBlocker::enableToggle()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("||ads.example.com^"));

    QSignalSpy spy(&blocker, &AdBlocker::enabledChanged);
    blocker.setEnabled(false);
    blocker.setEnabled(false);
    QCOMPARE(spy.count(), 1);

    blocker.setEnabled(true);
    QCOMPARE(spy.count(), 2);
}

void TestAdBlocker::countsRulesAndExceptions()
{
    AdBlocker blocker;
    QVERIFY(blocker.loadFiltersFromText(QStringLiteral(
        "||a.example^\n||b.example^\n@@||c.a.example^\nplain.example\n")));

    QCOMPARE(blocker.ruleCount(), 3);
    QCOMPARE(blocker.exceptionCount(), 1);
}

void TestAdBlocker::optionSuffixIsNotPartOfThePattern()
{
    auto blocker = blockerWith("||googlesyndication.com^$script,third-party");

    QVERIFY(blocks(*blocker, "https://pagead2.googlesyndication.com/pagead/js/gpt.js",
                   RequestType::Script, QStringLiteral("youtube.com")));
    QVERIFY(!blocks(*blocker, "https://pagead2.googlesyndication.com/pagead/js/gpt.js",
                    RequestType::Image, QStringLiteral("youtube.com")));
}

void TestAdBlocker::domainOptionLimitsRule()
{
    auto blocker = blockerWith("||ads.example.com^$domain=youtube.com");

    QVERIFY(blocks(*blocker, "https://ads.example.com/x.js", RequestType::Other,
                   QStringLiteral("youtube.com")));
    QVERIFY(!blocks(*blocker, "https://ads.example.com/x.js", RequestType::Other,
                    QStringLiteral("example.com")));
    QVERIFY(!blocks(*blocker, "https://ads.example.com/x.js", RequestType::Other));
}

void TestAdBlocker::negatedDomainOptionExcludes()
{
    auto blocker = blockerWith("||ads.example.com^$domain=~youtube.com");

    QVERIFY(!blocks(*blocker, "https://ads.example.com/x.js", RequestType::Other,
                    QStringLiteral("youtube.com")));
    QVERIFY(blocks(*blocker, "https://ads.example.com/x.js", RequestType::Other,
                   QStringLiteral("example.com")));
}

void TestAdBlocker::domainOptionRequiresDocument()
{
    auto blocker = blockerWith("||ads.example.com^$domain=youtube.com");
    QVERIFY(!blocks(*blocker, "https://ads.example.com/x.js"));
}

void TestAdBlocker::typeOptionSelectsResourceType()
{
    auto blocker = blockerWith("/tracker$image\n/script.js$script");

    QVERIFY(blocks(*blocker, "https://cdn.example.com/tracker", RequestType::Image));
    QVERIFY(!blocks(*blocker, "https://cdn.example.com/tracker", RequestType::Script));
    QVERIFY(blocks(*blocker, "https://cdn.example.com/script.js", RequestType::Script));
}

void TestAdBlocker::generichideExceptionDoesNotBlockNetwork()
{
    // EasyList ships `@@||www.youtube.com^$generichide`. That option only affects
    // element hiding, so treating it as a network exception would cancel the list's
    // own blocking rules for the whole site.
    auto blocker = blockerWith("||youtube.com/youtubei/v1/player/ad_break\n"
                               "@@||www.youtube.com^$generichide\n");

    QVERIFY(blocks(*blocker, "https://www.youtube.com/youtubei/v1/player/ad_break",
                   RequestType::Other, QStringLiteral("www.youtube.com")));
}

void TestAdBlocker::matchCaseOptionIsHonoured()
{
    auto blocker = blockerWith("/Promo$match-case");

    QVERIFY(blocks(*blocker, "https://site.example/Promo"));
    QVERIFY(!blocks(*blocker, "https://site.example/promo"));
}

void TestAdBlocker::unknownOptionFallsBackToLiteralPattern()
{
    auto blocker = blockerWith("||ads.example.com^$made-up-option");

    // An unrecognised option name leaves the whole line as a literal pattern, exactly
    // as Adblock Plus does, so the rule no longer matches the bare domain.
    QVERIFY(!blocks(*blocker, "https://ads.example.com/x.js"));
}

void TestAdBlocker::generatedPatternsMatchExpectedTargets()
{
    // The engine matches each pattern against the full URL and against the URL with
    // the scheme stripped; the assertions below cover both shapes.
    const auto matches = [](const char *filter, const char *url) {
        const QRegularExpression expression(
            AdBlocker::patternToRegExpSource(QString::fromLatin1(filter)),
            QRegularExpression::CaseInsensitiveOption);
        if (!expression.isValid())
            return false;

        const QString full = QString::fromLatin1(url);
        if (expression.match(full).hasMatch())
            return true;

        QString withoutScheme = full;
        withoutScheme.remove(0, withoutScheme.indexOf(QLatin1String("://")) + 3);
        return expression.match(withoutScheme).hasMatch();
    };

    QVERIFY(matches("||example.com^", "https://example.com/"));
    QVERIFY(matches("||example.com^", "https://cdn.example.com/x"));
    QVERIFY(!matches("||example.com^", "https://notexample.com/"));
    QVERIFY(matches("/ads?", "https://site.example/ads?x=1"));
    QVERIFY(!matches("/ads?", "https://site.example/adserver"));
    QVERIFY(matches("|https://site.example/api/", "https://site.example/api/v1"));
    QVERIFY(!matches("|https://site.example/api/", "http://cdn.site.example/api/v1"));
}

void TestAdBlocker::separatorBeforeOptions()
{
    auto blocker = blockerWith("||googlesyndication.com^$domain=blogto.com|youtube.com");

    QVERIFY(blocks(*blocker, "https://googlesyndication.com/pagead/x", RequestType::Other,
                   QStringLiteral("youtube.com")));
    QVERIFY(!blocks(*blocker, "https://googlesyndication.com/pagead/x", RequestType::Other,
                    QStringLiteral("example.com")));
}

void TestAdBlocker::endOfAddressQuestionMark()
{
    auto blocker = blockerWith("/api/stats/ads?");

    QVERIFY(blocks(*blocker, "https://www.youtube.com/api/stats/ads?html5=1"));
    QVERIFY(!blocks(*blocker, "https://www.youtube.com/api/stats/adsserver"));
    QVERIFY(!blocks(*blocker, "https://www.youtube.com/watch?v=abc"));
}

void TestAdBlocker::startAnchorMatchesScheme()
{
    auto blocker = blockerWith("|https://www.youtube.com/api/");

    QVERIFY(blocks(*blocker, "https://www.youtube.com/api/stats/ads"));
    QVERIFY(!blocks(*blocker, "https://other.youtube.com/api/stats/ads"));
}

void TestAdBlocker::cosmeticRulesAreParsed()
{
    AdBlocker blocker;
    QVERIFY(blocker.loadFiltersFromText(QStringLiteral("||ads.example^\n"
                                                       "##.generic-ad\n"
                                                       "example.com##.sponsored\n")));

    QCOMPARE(blocker.ruleCount(), 1);
    QCOMPARE(blocker.cosmeticRuleCount(), 2);
    QCOMPARE(blocker.cosmeticExceptionCount(), 0);
    QCOMPARE(blocker.ruleCount(), 1);
}

void TestAdBlocker::cosmeticDomainScoping()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("example.com##.sponsored\n##.generic-ad\n"));

    const QStringList onExample = blocker.cosmeticSelectors(QStringLiteral("example.com"));
    QVERIFY(onExample.contains(QStringLiteral(".sponsored")));
    QVERIFY(onExample.contains(QStringLiteral(".generic-ad")));

    const QStringList onOther = blocker.cosmeticSelectors(QStringLiteral("other.com"));
    QVERIFY(!onOther.contains(QStringLiteral(".sponsored")));
    QVERIFY(onOther.contains(QStringLiteral(".generic-ad")));
}

void TestAdBlocker::cosmeticExceptionsWin()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(
        QStringLiteral("##.generic-ad\nexample.com#@#.generic-ad\n"));

    const QStringList selectors = blocker.cosmeticSelectors(QStringLiteral("example.com"));
    QVERIFY(selectors.isEmpty());
    QVERIFY(!blocker.cosmeticSelectors(QStringLiteral("other.com")).isEmpty());
}

void TestAdBlocker::cosmeticGroupsCarryDomainScope()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("youtube.com##.ytp-ad-overlay\n"
                                               "youtube.com##.ytp-ad-slot\n"
                                               "##.generic-ad\n"
                                               "youtube.com#@#.ytp-ad-slot\n"));

    const QList<CosmeticGroup> groups = blocker.cosmeticGroups();
    QVERIFY(groups.size() >= 2);

    QString globalSelectors;
    QString youtubeHide;
    QString youtubeShow;
    for (const CosmeticGroup &group : groups) {
        if (group.isException && group.domains.contains(QStringLiteral("youtube.com"))) {
            youtubeShow = group.selectors.join(QLatin1Char(','));
        } else if (group.domains.isEmpty()) {
            globalSelectors = group.selectors.join(QLatin1Char(','));
        } else if (!group.isException) {
            youtubeHide = group.selectors.join(QLatin1Char(','));
        }
    }

    QVERIFY(youtubeHide.contains(QStringLiteral(".ytp-ad-overlay")));
    QVERIFY(youtubeShow.contains(QStringLiteral(".ytp-ad-slot")));
    QVERIFY(globalSelectors.contains(QStringLiteral(".generic-ad")));
}

void TestAdBlocker::cosmeticScriptHidesWithDomainScope()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("youtube.com##.ytp-ad-overlay\n"
                                               "youtube.com#@#.keep-me\n"
                                               "##.generic-ad\n"
                                               "##.keep-me\n"));

    const QString source = buildCosmeticSource(blocker.cosmeticGroups());
QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("display:none!important")));
    QVERIFY(source.contains(QStringLiteral("location.hostname")));
    QVERIFY(source.contains(QStringLiteral(".ytp-ad-overlay")));
    QVERIFY(source.contains(QStringLiteral(".keep-me")));

    // The sheet text is already a joined string; a stray `.join()` here would throw
    // inside the injected world, where nothing reports it.
    QVERIFY(source.contains(QStringLiteral("sheet.textContent = css;")));
    QVERIFY(!source.contains(QStringLiteral("css.join")));
    QVERIFY(source.contains(QStringLiteral("location.hostname")));
}

void TestAdBlocker::cosmeticScriptIsEmptyWithoutRules()
{
    AdBlocker blocker;
    blocker.loadFiltersFromText(QStringLiteral("||ads.example.com^"));
    QVERIFY(buildCosmeticSource(blocker.cosmeticGroups()).isEmpty());
}

void TestAdBlocker::youtubeInlineAdRequestsAreBlocked_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("expectBlocked");

    QTest::newRow("pagead")
        << "https://www.youtube.com/pagead/xps/ads/x.js" << true;
    QTest::newRow("pagead host root")
        << "https://pagead2.googlesyndication.com/pagead/js/gpt.js" << true;
    QTest::newRow("ad_break")
        << "https://www.youtube.com/youtubei/v1/player/ad_break" << true;
    QTest::newRow("get_midroll_")
        << "https://m.youtube.com/get_midroll_?ad=1" << true;
    QTest::newRow("premium_ads")
        << "https://www.youtube.com/premium_ads" << true;
    QTest::newRow("stats ads")
        << "https://www.youtube.com/api/stats/ads?html5=1" << true;
    QTest::newRow("doubleclick")
        << "https://ad.doubleclick.net/ddm/ad.js" << true;

    QTest::newRow("watch page") << "https://www.youtube.com/watch?v=dQw4w9WgXcQ" << false;
    QTest::newRow("video segments")
        << "https://rr1---sn-abc.googlevideo.com/videoplayback?id=1" << false;
    QTest::newRow("home page") << "https://www.youtube.com/" << false;
}

void TestAdBlocker::youtubeInlineAdRequestsAreBlocked()
{
    QFETCH(QString, url);
    QFETCH(bool, expectBlocked);

    AdBlocker blocker;
    QVERIFY(blocker.loadFilters(QStringLiteral(SPACEPENGUIN_TEST_FILTER_LIST)));

    QCOMPARE(blocks(blocker, url.toUtf8().constData(), RequestType::Other,
                    QStringLiteral("www.youtube.com")),
             expectBlocked);
}

void TestAdBlocker::youtubeCosmeticRulesHideOverlay()
{
    AdBlocker blocker;
    QVERIFY(blocker.loadFilters(QStringLiteral(SPACEPENGUIN_TEST_FILTER_LIST)));

    QVERIFY(blocker.cosmeticRuleCount() > 0);
    const QStringList selectors =
        blocker.cosmeticSelectors(QStringLiteral("www.youtube.com"));
    QVERIFY(selectors.contains(QStringLiteral(".ytp-ad-overlay")));
    QVERIFY(selectors.contains(QStringLiteral(".ytp-ad-slot")));
    QVERIFY(!buildCosmeticSource(blocker.cosmeticGroups()).isEmpty());
}

void TestAdBlocker::popupAdsAreBlockedAsDocuments()
{
    auto blocker = blockerWith("||popads.example^\n||ads.example^$popup");

    QVERIFY(blocks(*blocker, "https://popads.example/landing.html", RequestType::Document));
    QVERIFY(blocks(*blocker, "https://ads.example/popup.html", RequestType::Popup,
                   QStringLiteral("example.com")));
}

void TestAdBlocker::sameHostContentIsNotBlocked()
{
    AdBlocker blocker;
    QVERIFY(blocker.loadFilters(QStringLiteral(SPACEPENGUIN_TEST_FILTER_LIST)));

    QVERIFY(!blocks(blocker, "https://www.youtube.com/watch?v=abc", RequestType::Document,
                    QStringLiteral("www.youtube.com")));
    QVERIFY(!blocks(blocker, "https://i.ytimg.com/vi/abc/hqdefault.jpg", RequestType::Image,
                    QStringLiteral("www.youtube.com")));
}

QTEST_MAIN(TestAdBlocker)
#include "tst_adblocker.moc"