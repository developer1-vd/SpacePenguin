#include <QtTest>

#include "startpageschemehandler.h"
#include "store.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace spacepenguin;

class TestStartPage : public QObject
{
    Q_OBJECT

private slots:
    void fillsPlaceholder();
    void escapesScriptBreakout();
    void dropsNonWebTiles();
    void bundledPageHasPlaceholder();
    void storeReadsGreasyFork();
    void storeReadsOpenUserJs();
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

void TestStartPage::storeReadsGreasyFork()
{
    // Shaped like api.greasyfork.org/en/scripts.json; a library (.js) is left out.
    const QByteArray json = R"([
      {"id": 1, "name": "Return YouTube Dislike", "description": "Shows dislikes",
       "total_installs": 2500000, "url": "https://greasyfork.org/scripts/1-ryd",
       "code_url": "https://update.greasyfork.org/scripts/1/RYD.user.js",
       "users": [{"name": "Anarios"}], "deleted": false},
      {"id": 2, "name": "A library", "total_installs": 9,
       "code_url": "https://update.greasyfork.org/scripts/2/lib.js", "users": []}
    ])";
    const QJsonArray results = spacepenguin::Store::parseGreasyFork(json);
    QCOMPARE(results.size(), 1);
    const QJsonObject script = results.first().toObject();
    QCOMPARE(script.value("name").toString(), QStringLiteral("Return YouTube Dislike"));
    QCOMPARE(script.value("author").toString(), QStringLiteral("Anarios"));
    QCOMPARE(script.value("installs").toInteger(), 2500000);
    QCOMPARE(script.value("source").toString(), QStringLiteral("Greasy Fork"));
    QVERIFY(script.value("install").toString().endsWith(".user.js"));
}

void TestStartPage::storeReadsOpenUserJs()
{
    // A row of openuserjs.org's search results, as the site renders it.
    const QByteArray html = R"(
      <tr class="tr-link">
        <td>
          <span class="script-icon hidden-xs"><i class="fa fa-fw fa-file-code-o"></i></span>
          <a href="/scripts/cuzi/Youtube_Genius_Lyrics" class="tr-link-a"><b>Youtube Genius Lyrics &amp; More</b></a>
          <span class="script-version label label-default">10.10.19</span>
          <span class="inline-block">by <a href="/users/cuzi">cuzi</a></span>
          <p>Shows lyrics from genius.com on Youtube</p>
        </td>
        <td class="text-center td-fit">
          <p>12001</p>
        </td>
      </tr>)";
    const QJsonArray results = spacepenguin::Store::parseOpenUserJs(html);
    QCOMPARE(results.size(), 1);
    const QJsonObject script = results.first().toObject();
    QCOMPARE(script.value("name").toString(), QStringLiteral("Youtube Genius Lyrics & More"));
    QCOMPARE(script.value("author").toString(), QStringLiteral("cuzi"));
    QCOMPARE(script.value("installs").toInteger(), 12001);
    QCOMPARE(script.value("install").toString(),
             QStringLiteral("https://openuserjs.org/install/cuzi/Youtube_Genius_Lyrics.user.js"));
}

QTEST_MAIN(TestStartPage)
#include "tst_startpage.moc"
