#include <QtTest>

#include "urlresolver.h"

using namespace spacepenguin;

class TestUrlResolver : public QObject
{
    Q_OBJECT

private slots:
    void looksLikeUrl_data();
    void looksLikeUrl();
    void resolve_data();
    void resolve();
    void searchUrl();
    void homeUrl();
    void percentEncodesQueries();
};

void TestUrlResolver::looksLikeUrl_data()
{
    QTest::addColumn<QString>("input");

    QTest::newRow("scheme https") << QStringLiteral("https://example.com");
    QTest::newRow("scheme http") << QStringLiteral("http://example.com");
    QTest::newRow("scheme about") << QStringLiteral("about:blank");
    QTest::newRow("scheme qrc") << QStringLiteral("qrc:/start.html");
    QTest::newRow("bare host") << QStringLiteral("example.com");
    QTest::newRow("bare host path") << QStringLiteral("example.com/path?q=1");
    QTest::newRow("bare host port") << QStringLiteral("example.com:8080");
    QTest::newRow("localhost") << QStringLiteral("localhost");
    QTest::newRow("localhost port") << QStringLiteral("localhost:3000");
    QTest::newRow("ipv4") << QStringLiteral("127.0.0.1");
    QTest::newRow("protocol relative") << QStringLiteral("//example.com");

    QTest::newRow("two words") << QStringLiteral("how to bake bread");
    QTest::newRow("single word") << QStringLiteral("penguins");
    QTest::newRow("no tld") << QStringLiteral("example.c");
    QTest::newRow("empty") << QString();
    QTest::newRow("trailing dot") << QStringLiteral("example.com.");
    QTest::newRow("underscore host") << QStringLiteral("my_host.example.com");
    QTest::newRow("sentence with dot") << QStringLiteral("load example.com now");
}

void TestUrlResolver::looksLikeUrl()
{
    QFETCH(QString, input);

    const bool expected = input.startsWith(QStringLiteral("https"))
        || input.startsWith(QStringLiteral("http"))
        || input.startsWith(QStringLiteral("about"))
        || input.startsWith(QStringLiteral("qrc"))
        || input == QStringLiteral("example.com")
        || input == QStringLiteral("example.com/path?q=1")
        || input == QStringLiteral("example.com:8080")
        || input == QStringLiteral("localhost")
        || input == QStringLiteral("localhost:3000")
        || input == QStringLiteral("127.0.0.1")
        || input == QStringLiteral("//example.com")
        || input == QStringLiteral("my_host.example.com");

    QCOMPARE(UrlResolver::looksLikeUrl(input), expected);
}

void TestUrlResolver::resolve_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty is home") << QString() << QStringLiteral("sp://start/");
    QTest::newRow("https kept") << QStringLiteral("https://example.com")
                                << QStringLiteral("https://example.com");
    QTest::newRow("http kept") << QStringLiteral("http://example.com")
                               << QStringLiteral("http://example.com");
    QTest::newRow("host upgraded") << QStringLiteral("example.com")
                                   << QStringLiteral("https://example.com");
    QTest::newRow("host with path") << QStringLiteral("example.com/a")
                                    << QStringLiteral("https://example.com/a");
    QTest::newRow("localhost") << QStringLiteral("localhost:3000")
                               << QStringLiteral("https://localhost:3000");
    QTest::newRow("protocol relative") << QStringLiteral("//example.com")
                                       << QStringLiteral("https://example.com");
    QTest::newRow("words search") << QStringLiteral("how to bake bread")
                                  << QStringLiteral("https://duckduckgo.com/?q=how%20to%20bake%20bread");
}

void TestUrlResolver::resolve()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    const UrlResolver resolver;
    QCOMPARE(resolver.resolve(input).toString(QUrl::FullyEncoded), expected);
}

void TestUrlResolver::searchUrl()
{
    const UrlResolver resolver;
    QCOMPARE(resolver.searchUrl(QStringLiteral("qt webengine")).toString(QUrl::FullyEncoded),
             QStringLiteral("https://duckduckgo.com/?q=qt%20webengine"));
}

void TestUrlResolver::homeUrl()
{
    const UrlResolver resolver;
    QCOMPARE(resolver.homeUrl().toString(QUrl::FullyEncoded), UrlResolver::defaultHomeUrl());
    QCOMPARE(UrlResolver::defaultHomeUrl(), QStringLiteral("sp://start/"));
}

void TestUrlResolver::percentEncodesQueries()
{
    const UrlResolver resolver;
    const QUrl url = resolver.resolve(QStringLiteral("a&b=c"));
    QCOMPARE(url.host(), QStringLiteral("duckduckgo.com"));
    QCOMPARE(url.query(QUrl::FullyEncoded), QStringLiteral("q=a%26b%3Dc"));
}

QTEST_MAIN(TestUrlResolver)
#include "tst_urlresolver.moc"