#include <QtTest>

#include "aboutpages.h"

using namespace spacepenguin;

static AboutPageContext sampleContext()
{
    AboutPageContext context;
    context.appVersion = QStringLiteral("0.1.0");
    context.buildType = QStringLiteral("test");
    context.platform = QStringLiteral("offscreen");
    context.theme = QStringLiteral("System");
    return context;
}

class TestAboutPages : public QObject
{
    Q_OBJECT

private slots:
    void indexListsEveryPage();
    void indexLinksEveryPage();
    void knownPagesRender();
    void knownPagesRender_data();
    void unknownPagesRender();
    void unknownPageHasNoTitle();
    void pageTitles();
    void versionPageReportsPlatform();
    void versionPageReportsFeatures();
    void versionPageAlwaysReportsPlatform();
    void htmlShellEscapesTitles();
    void historyPageListsEntries();
    void historyPageEscapesEntries();
    void historyPageIsEmptyInPrivate();
    void downloadsPageListsEntries();
};

void TestAboutPages::indexListsEveryPage()
{
    const QList<AboutPage> pages = AboutPages::all();
    QVERIFY(pages.size() >= 6);

    QStringList ids;
    for (const AboutPage &page : pages) {
        ids.append(page.id);
        QVERIFY(!page.id.isEmpty());
        QVERIFY(!page.title.isEmpty());
        QVERIFY(!page.summary.isEmpty());
    }

    QVERIFY(ids.contains(QStringLiteral("about:about")));
    QVERIFY(ids.contains(QStringLiteral("about:version")));
    QVERIFY(ids.contains(QStringLiteral("about:license")));
    QVERIFY(AboutPages::ids().size() == ids.size());
}

void TestAboutPages::indexLinksEveryPage()
{
    const QString html = AboutPages::render(QStringLiteral("about:about"), sampleContext());

    for (const AboutPage &page : AboutPages::all()) {
        if (page.id == QLatin1String("about:about"))
            continue;
        QVERIFY2(html.contains(QStringLiteral("href=\"%1\"").arg(page.id)),
                 qPrintable(QStringLiteral("about:about does not link to %1").arg(page.id)));
    }

    QVERIFY(html.contains(QStringLiteral("easter egg")));
    QVERIFY(html.contains(QStringLiteral("about:blank")));
}

void TestAboutPages::knownPagesRender_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("needle");

    QTest::newRow("about") << QStringLiteral("about:about") << QStringLiteral("Internal pages");
    QTest::newRow("version") << QStringLiteral("about:version") << QStringLiteral("Renderer");
    QTest::newRow("license") << QStringLiteral("about:license") << QStringLiteral("GNU General Public License v3");
    QTest::newRow("penguin") << QStringLiteral("about:penguin") << QStringLiteral("127.0.0.1");
    QTest::newRow("teapot") << QStringLiteral("about:teapot") << QStringLiteral("418");
    QTest::newRow("pan") << QStringLiteral("about:pan") << QStringLiteral("panned");
}

void TestAboutPages::knownPagesRender()
{
    QFETCH(QString, id);
    QFETCH(QString, needle);

    QVERIFY(AboutPages::isKnown(id));
    const QString html = AboutPages::render(id, sampleContext());
    QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(needle));
    QVERIFY(html.contains(QStringLiteral("</html>")));
}

void TestAboutPages::unknownPagesRender()
{
    QVERIFY(!AboutPages::isKnown(QStringLiteral("about:definitely-not-real")));

    const QString html =
        AboutPages::render(QStringLiteral("about:nope"), sampleContext());
    QVERIFY(html.contains(QStringLiteral("No such page")));
    QVERIFY(html.contains(QStringLiteral("about:nope")));
    QVERIFY(html.contains(QStringLiteral("href=\"about:about\"")));
}

void TestAboutPages::unknownPageHasNoTitle()
{
    QVERIFY(AboutPages::page(QStringLiteral("about:nope")).id.isEmpty());
    QCOMPARE(AboutPages::pageTitle(QStringLiteral("about:nope")),
             QStringLiteral("Page not found"));
}

void TestAboutPages::pageTitles()
{
    QCOMPARE(AboutPages::pageTitle(QStringLiteral("about:about")),
             QStringLiteral("About SpacePenguin"));
    QCOMPARE(AboutPages::pageTitle(QStringLiteral("about:penguin")), QStringLiteral("Penguin"));
    QCOMPARE(AboutPages::pageTitle(QStringLiteral("ABOUT:VERSION")), QStringLiteral("Version"));
}

void TestAboutPages::versionPageReportsPlatform()
{
    const QString html =
        AboutPages::render(QStringLiteral("about:version"), sampleContext());
    QVERIFY(html.contains(QStringLiteral("Platform")));
    QVERIFY(html.contains(QStringLiteral("offscreen")));
    QVERIFY(html.contains(QStringLiteral("0.1.0")));

    AboutPageContext privateContext = sampleContext();
    privateContext.isPrivate = true;
    QVERIFY(AboutPages::render(QStringLiteral("about:version"), privateContext)
                .contains(QStringLiteral("Private")));

    AboutPageContext persistentContext = sampleContext();
    QVERIFY(AboutPages::render(QStringLiteral("about:version"), persistentContext)
                .contains(QStringLiteral("Persistent")));
}

void TestAboutPages::versionPageReportsFeatures()
{
    AboutPageContext context = sampleContext();
    context.filterRules = 42;
    context.blockingEnabled = true;
    context.blockedRequests = 7;
    context.extensionCount = 3;
    context.theme = QStringLiteral("Dark");

    const QString html = AboutPages::render(QStringLiteral("about:version"), context);
    QVERIFY(html.contains(QStringLiteral("42")));
    QVERIFY(html.contains(QStringLiteral("7")));
    QVERIFY(html.contains(QStringLiteral("3")));
    QVERIFY(html.contains(QStringLiteral("Dark")));

    context.blockingEnabled = false;
    QVERIFY(AboutPages::render(QStringLiteral("about:version"), context)
                .contains(QStringLiteral("Disabled")));
}

void TestAboutPages::versionPageAlwaysReportsPlatform()
{
    AboutPageContext context = sampleContext();
    context.platform.clear();

    const QString html = AboutPages::render(QStringLiteral("about:version"), context);
    QVERIFY(html.contains(QStringLiteral("Platform")));
    QVERIFY(!html.contains(QStringLiteral("<td></td>")));
}

void TestAboutPages::htmlShellEscapesTitles()
{
    const QString html = AboutPages::htmlShell(QStringLiteral("<script>alert(1)</script>"),
                                               QStringLiteral("body"));
    QVERIFY(!html.contains(QStringLiteral("<script>")));
    QVERIFY(html.contains(QStringLiteral("&lt;script&gt;")));
}

void TestAboutPages::historyPageListsEntries()
{
    AboutPageContext context = sampleContext();
    context.history.append(AboutPageEntry{QStringLiteral("Example Domain"),
                                          QStringLiteral("https://example.com/"),
                                          QStringLiteral("today")});

    const QString html = AboutPages::render(QStringLiteral("about:history"), context);
    QVERIFY(html.contains(QStringLiteral("href=\"https://example.com/\"")));
    QVERIFY(html.contains(QStringLiteral("Example Domain")));
    QVERIFY(html.contains(QStringLiteral("today")));
}

void TestAboutPages::historyPageEscapesEntries()
{
    AboutPageContext context = sampleContext();
    context.history.append(AboutPageEntry{QStringLiteral("<script>alert(1)</script>"),
                                          QStringLiteral("javascript:alert(1)"),
                                          QStringLiteral("<b>now</b>")});

    const QString html = AboutPages::render(QStringLiteral("about:history"), context);
    QVERIFY(!html.contains(QStringLiteral("<script>")));
    QVERIFY(!html.contains(QStringLiteral("<b>now</b>")));
    // Only web addresses become links.
    QVERIFY(!html.contains(QStringLiteral("href=\"javascript:")));
}

void TestAboutPages::historyPageIsEmptyInPrivate()
{
    AboutPageContext context = sampleContext();
    context.isPrivate = true;
    const QString html = AboutPages::render(QStringLiteral("about:history"), context);
    QVERIFY(html.contains(QStringLiteral("private window")));
}

void TestAboutPages::downloadsPageListsEntries()
{
    AboutPageContext context = sampleContext();
    QVERIFY(AboutPages::render(QStringLiteral("about:downloads"), context)
                .contains(QStringLiteral("Nothing downloaded")));

    context.downloads.append(AboutPageEntry{QStringLiteral("report.pdf"),
                                            QStringLiteral("https://example.com/report.pdf"),
                                            QStringLiteral("Completed")});
    const QString html = AboutPages::render(QStringLiteral("about:downloads"), context);
    QVERIFY(html.contains(QStringLiteral("report.pdf")));
    QVERIFY(html.contains(QStringLiteral("Completed")));
}

QTEST_MAIN(TestAboutPages)
#include "tst_aboutpages.moc"