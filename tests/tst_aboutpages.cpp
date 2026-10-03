#include <QtTest>

#include "aboutpages.h"

using namespace spacepenguin;

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
    void htmlShellEscapesTitles();
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
    const QString html = AboutPages::render(QStringLiteral("about:about"), QStringLiteral("0.1.0"),
                                            false);

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
    QTest::newRow("license") << QStringLiteral("about:license") << QStringLiteral("BSD 3-Clause");
    QTest::newRow("penguin") << QStringLiteral("about:penguin") << QStringLiteral("127.0.0.1");
    QTest::newRow("teapot") << QStringLiteral("about:teapot") << QStringLiteral("418");
    QTest::newRow("pan") << QStringLiteral("about:pan") << QStringLiteral("panned");
}

void TestAboutPages::knownPagesRender()
{
    QFETCH(QString, id);
    QFETCH(QString, needle);

    QVERIFY(AboutPages::isKnown(id));
    const QString html = AboutPages::render(id, QStringLiteral("0.1.0"), false);
    QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(needle));
    QVERIFY(html.contains(QStringLiteral("</html>")));
}

void TestAboutPages::unknownPagesRender()
{
    QVERIFY(!AboutPages::isKnown(QStringLiteral("about:definitely-not-real")));

    const QString html =
        AboutPages::render(QStringLiteral("about:nope"), QStringLiteral("0.1.0"), false);
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

void TestAboutPages::htmlShellEscapesTitles()
{
    const QString html = AboutPages::htmlShell(QStringLiteral("<script>alert(1)</script>"),
                                               QStringLiteral("body"));
    QVERIFY(!html.contains(QStringLiteral("<script>")));
    QVERIFY(html.contains(QStringLiteral("&lt;script&gt;")));
}

QTEST_MAIN(TestAboutPages)
#include "tst_aboutpages.moc"