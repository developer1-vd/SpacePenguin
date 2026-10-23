#include <QtTest>

#include "preferences.h"
#include "roles.h"
#include "tor.h"
#include "webapps.h"

using namespace spacepenguin;

class TestRoles : public QObject
{
    Q_OBJECT

private slots:
    void focusModeComesWithSomeRoles();
    void normalizeSite_data();
    void normalizeSite();
    void blocksSitesAndSubdomains();
    void citation();
    void manifestNameStartAndScope();
    void manifestPicksBestIcon();
    void manifestStaysOnItsSite();
    void appContainsItsScope();
    void desktopEntryQuotesAndNamesTheApp();
    void searchTemplates();
    void onions();
};

void TestRoles::focusModeComesWithSomeRoles()
{
    QVERIFY(Roles::hasFocusMode({QStringLiteral("student")}));
    QVERIFY(Roles::hasFocusMode({QStringLiteral("gamer"), QStringLiteral("teacher")}));
    QVERIFY(!Roles::hasFocusMode({QStringLiteral("developer"), QStringLiteral("gamer")}));
    QVERIFY(!Roles::hasFocusMode({}));
}

void TestRoles::normalizeSite_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("plain") << QStringLiteral("youtube.com") << QStringLiteral("youtube.com");
    QTest::newRow("www and case") << QStringLiteral(" WWW.YouTube.com/ ") << QStringLiteral("youtube.com");
    QTest::newRow("address") << QStringLiteral("https://www.reddit.com/r/all") << QStringLiteral("reddit.com");
    QTest::newRow("empty") << QStringLiteral("   ") << QString();
}

void TestRoles::normalizeSite()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(Roles::normalizeSite(input), expected);
}

void TestRoles::blocksSitesAndSubdomains()
{
    const QStringList sites = {QStringLiteral("youtube.com"), QStringLiteral("x.com")};
    QVERIFY(Roles::isBlocked(QUrl(QStringLiteral("https://www.youtube.com/watch?v=1")), sites));
    QVERIFY(Roles::isBlocked(QUrl(QStringLiteral("https://m.youtube.com/")), sites));
    QVERIFY(Roles::isBlocked(QUrl(QStringLiteral("http://x.com/home")), sites));
    QVERIFY(!Roles::isBlocked(QUrl(QStringLiteral("https://notyoutube.com/")), sites));
    QVERIFY(!Roles::isBlocked(QUrl(QStringLiteral("https://box.com/")), sites)); // not "x.com"
    QVERIFY(!Roles::isBlocked(QUrl(QStringLiteral("sp://start/")), sites));
}

void TestRoles::citation()
{
    const QString text = Roles::citation(QStringLiteral("Penguins"),
                                         QUrl(QStringLiteral("https://www.example.org/birds/penguins")),
                                         QDate(2026, 10, 21));
    QCOMPARE(text, QStringLiteral("Penguins. example.org. https://www.example.org/birds/penguins "
                                  "(accessed 21 October 2026)."));
}

void TestRoles::manifestNameStartAndScope()
{
    const QByteArray json = R"({"name": "Example Mail", "short_name": "Mail",
                                "start_url": "/inbox?source=pwa", "scope": "/",
                                "theme_color": "#123456"})";
    const WebApp app = WebApps::fromManifest(json, QUrl(QStringLiteral("https://mail.example.com/app.webmanifest")),
                                             QUrl(QStringLiteral("https://mail.example.com/inbox/42")));
    QCOMPARE(app.name, QStringLiteral("Example Mail"));
    QCOMPARE(app.startUrl, QUrl(QStringLiteral("https://mail.example.com/inbox?source=pwa")));
    QCOMPARE(app.scope, QUrl(QStringLiteral("https://mail.example.com/")));
    QCOMPARE(app.themeColor, QStringLiteral("#123456"));
    QVERIFY(app.id.startsWith(QStringLiteral("mail-example-com-")));

    // Without a name or a start page: the short name, the page itself.
    const WebApp bare = WebApps::fromManifest(R"({"short_name": "Notes"})",
                                              QUrl(QStringLiteral("https://notes.example.com/m.json")),
                                              QUrl(QStringLiteral("https://notes.example.com/edit/today")));
    QCOMPARE(bare.name, QStringLiteral("Notes"));
    QCOMPARE(bare.startUrl, QUrl(QStringLiteral("https://notes.example.com/edit/today")));
    QCOMPARE(bare.scope, QUrl(QStringLiteral("https://notes.example.com/edit/")));
}

void TestRoles::manifestPicksBestIcon()
{
    const QByteArray json = R"({"name": "A", "icons": [
        {"src": "/i/48.png", "sizes": "48x48"},
        {"src": "/i/any.svg", "sizes": "any", "type": "image/svg+xml"},
        {"src": "/i/512-mask.png", "sizes": "512x512", "purpose": "maskable"},
        {"src": "/i/192.png", "sizes": "192x192"},
        {"src": "/i/1024.png", "sizes": "1024x1024"}]})";
    QUrl icon;
    WebApps::fromManifest(json, QUrl(QStringLiteral("https://a.example/manifest.json")),
                          QUrl(QStringLiteral("https://a.example/")), &icon);
    QCOMPARE(icon, QUrl(QStringLiteral("https://a.example/i/192.png")));

    const QByteArray small = R"({"icons": [{"src": "big.png", "sizes": "2048x2048"},
                                           {"src": "mid.png", "sizes": "256x256"}]})";
    WebApps::fromManifest(small, QUrl(QStringLiteral("https://a.example/static/m.json")),
                          QUrl(QStringLiteral("https://a.example/")), &icon);
    QCOMPARE(icon, QUrl(QStringLiteral("https://a.example/static/mid.png")));
}

void TestRoles::manifestStaysOnItsSite()
{
    const QByteArray json = R"({"start_url": "https://evil.example/", "scope": "https://evil.example/"})";
    const WebApp app = WebApps::fromManifest(json, QUrl(QStringLiteral("https://good.example/m.json")),
                                             QUrl(QStringLiteral("https://good.example/home")));
    QCOMPARE(app.startUrl.host(), QStringLiteral("good.example"));
    QCOMPARE(app.scope.host(), QStringLiteral("good.example"));
}

void TestRoles::appContainsItsScope()
{
    WebApp app;
    app.startUrl = QUrl(QStringLiteral("https://example.com/app/start"));
    app.scope = QUrl(QStringLiteral("https://example.com/app/"));
    QVERIFY(app.contains(QUrl(QStringLiteral("https://example.com/app/settings"))));
    QVERIFY(!app.contains(QUrl(QStringLiteral("https://example.com/blog"))));
    QVERIFY(!app.contains(QUrl(QStringLiteral("https://other.example/app/"))));
    QVERIFY(!app.contains(QUrl(QStringLiteral("http://example.com/app/"))));
}

void TestRoles::desktopEntryQuotesAndNamesTheApp()
{
    WebApp app;
    app.id = QStringLiteral("example-com-abc123");
    app.name = QStringLiteral("Example 100%");
    app.startUrl = QUrl(QStringLiteral("https://example.com/"));
    const QString entry = WebApps::desktopEntry(app, QStringLiteral("/opt/Space Penguin/AppRun"),
                                                QStringLiteral("/icons/a.png"));
    QVERIFY(entry.startsWith(QStringLiteral("[Desktop Entry]\n")));
    QVERIFY(entry.contains(QStringLiteral("\nName=Example 100%\n")));
    QVERIFY(entry.contains(QStringLiteral("\nExec=\"/opt/Space Penguin/AppRun\" --app=example-com-abc123\n")));
    QVERIFY(entry.contains(QStringLiteral("\nIcon=/icons/a.png\n")));
}

void TestRoles::searchTemplates()
{
    QCOMPARE(Preferences::normalizeSearchTemplate(QStringLiteral(" https://example.com/find?q=%s ")),
             QStringLiteral("https://example.com/find?q={query}"));
    QCOMPARE(Preferences::normalizeSearchTemplate(QStringLiteral("https://example.com/?q={query}")),
             QStringLiteral("https://example.com/?q={query}"));
    QVERIFY(Preferences::normalizeSearchTemplate(QStringLiteral("https://example.com/")).isEmpty());
    QVERIFY(Preferences::normalizeSearchTemplate(QStringLiteral("javascript:alert(%s)")).isEmpty());
    QVERIFY(Preferences::normalizeSearchTemplate(QStringLiteral("example.com/?q=%s")).isEmpty());
}

void TestRoles::onions()
{
    QVERIFY(Tor::isOnion(QUrl(QStringLiteral("http://abc.onion/path"))));
    QVERIFY(Tor::isOnion(QUrl(QStringLiteral("https://www.ABC.ONION/"))));
    QVERIFY(!Tor::isOnion(QUrl(QStringLiteral("https://onion.example.com/"))));
    QCOMPARE(Tor::searchTemplate(QStringLiteral("https://duckduckgo.com/?q={query}")).section(QLatin1Char('/'), 2, 2)
                 .endsWith(QLatin1String(".onion")),
             true);
    QCOMPARE(Tor::searchTemplate(QStringLiteral("https://www.bing.com/search?q={query}")),
             QStringLiteral("https://www.bing.com/search?q={query}"));
}

QTEST_GUILESS_MAIN(TestRoles)
#include "tst_roles.moc"
