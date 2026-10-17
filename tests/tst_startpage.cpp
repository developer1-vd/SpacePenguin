#include <QtTest>

#include "startpageschemehandler.h"

using namespace spacepenguin;

class TestStartPage : public QObject
{
    Q_OBJECT

private slots:
    void fillsPlaceholder();
    void escapesScriptBreakout();
    void dropsNonWebTiles();
    void bundledPageHasPlaceholder();
};

static const QByteArray kTemplate =
    QByteArrayLiteral("<html><script>var data = /*SP_DATA*/null;</script></html>");

static QJsonObject extractData(const QByteArray &page)
{
    const int start = page.indexOf("var data = ") + 11;
    const int end = page.indexOf(";</script>");
    return QJsonDocument::fromJson(page.mid(start, end - start)).object();
}

void TestStartPage::fillsPlaceholder()
{
    const QByteArray page = StartPageSchemeHandler::renderPage(
        kTemplate, QStringLiteral("https://search.example/?q={query}"),
        {StartPageTile{QStringLiteral("Example"), QStringLiteral("https://example.com/")}}, true);

    QVERIFY(!page.contains("/*SP_DATA*/"));
    const QJsonObject data = extractData(page);
    QCOMPARE(data.value(QStringLiteral("search")).toString(),
             QStringLiteral("https://search.example/?q={query}"));
    QCOMPARE(data.value(QStringLiteral("private")).toBool(), true);
    QCOMPARE(data.value(QStringLiteral("tiles")).toArray().size(), 1);

    const QJsonArray shortcuts = data.value(QStringLiteral("shortcuts")).toArray();
    QVERIFY(!shortcuts.isEmpty());
    for (const QJsonValue &shortcut : shortcuts)
        QVERIFY(!shortcut.toObject().value(QStringLiteral("keys")).toString().isEmpty());
}

void TestStartPage::escapesScriptBreakout()
{
    const QByteArray page = StartPageSchemeHandler::renderPage(
        kTemplate, QStringLiteral("https://duckduckgo.com/?q={query}"),
        {StartPageTile{QStringLiteral("</script><script>alert(1)</script>"),
                       QStringLiteral("https://evil.example/")}},
        false);

    // Exactly the template's own script element, nothing injected by the title.
    QCOMPARE(page.count("</script>"), 1);
    QCOMPARE(extractData(page).value(QStringLiteral("tiles")).toArray().first().toObject()
                 .value(QStringLiteral("title")).toString(),
             QStringLiteral("</script><script>alert(1)</script>"));
}

void TestStartPage::dropsNonWebTiles()
{
    const QByteArray page = StartPageSchemeHandler::renderPage(
        kTemplate, QStringLiteral("https://duckduckgo.com/?q={query}"),
        {StartPageTile{QStringLiteral("bad"), QStringLiteral("javascript:alert(1)")},
         StartPageTile{QStringLiteral("file"), QStringLiteral("file:///etc/passwd")},
         StartPageTile{QStringLiteral("good"), QStringLiteral("http://example.org/")}},
        false);

    const QJsonArray tiles = extractData(page).value(QStringLiteral("tiles")).toArray();
    QCOMPARE(tiles.size(), 1);
    QCOMPARE(tiles.first().toObject().value(QStringLiteral("title")).toString(),
             QStringLiteral("good"));
}

void TestStartPage::bundledPageHasPlaceholder()
{
    QFile file(QStringLiteral(SPACEPENGUIN_START_PAGE));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("/*SP_DATA*/null"));
}

QTEST_MAIN(TestStartPage)
#include "tst_startpage.moc"
