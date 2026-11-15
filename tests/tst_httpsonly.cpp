#include <QtTest>

#include "httpsonly.h"

using namespace spacepenguin;

// Which addresses HTTPS-only mode upgrades, and the warning page's escaping.
class TestHttpsOnly : public QObject
{
    Q_OBJECT

private slots:
    void cleanup() { HttpsOnly::clearAllowed(); }

    void upgrades_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("upgrade");
        QTest::newRow("plain http") << "http://example.com/a?b=1" << true;
        QTest::newRow("already https") << "https://example.com/" << false;
        QTest::newRow("localhost") << "http://localhost:3000/" << false;
        QTest::newRow("ipv4") << "http://192.168.1.1/" << false;
        QTest::newRow("ipv6") << "http://[::1]:8080/" << false;
        QTest::newRow("mdns") << "http://printer.local/" << false;
        QTest::newRow("single name") << "http://nas/" << false;
        QTest::newRow("onion") << "http://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion/" << false;
        QTest::newRow("other scheme") << "ftp://example.com/" << false;
        QTest::newRow("sp page") << "sp://start/" << false;
    }

    void upgrades()
    {
        QFETCH(QString, url);
        QFETCH(bool, upgrade);
        QCOMPARE(HttpsOnly::shouldUpgrade(QUrl(url)), upgrade);
    }

    void upgradedAddress()
    {
        QCOMPARE(HttpsOnly::upgraded(QUrl(QStringLiteral("http://example.com/a?b=1#c"))),
                 QUrl(QStringLiteral("https://example.com/a?b=1#c")));
        QCOMPARE(HttpsOnly::upgraded(QUrl(QStringLiteral("http://example.com:80/"))),
                 QUrl(QStringLiteral("https://example.com/")));
        QCOMPARE(HttpsOnly::upgraded(QUrl(QStringLiteral("http://example.com:8080/"))),
                 QUrl(QStringLiteral("https://example.com:8080/")));
    }

    void continuingLetsTheSiteThrough()
    {
        const QUrl url(QStringLiteral("http://Old.Example/"));
        QVERIFY(HttpsOnly::shouldUpgrade(url));
        HttpsOnly::allow(QStringLiteral("old.example"));
        QVERIFY(!HttpsOnly::shouldUpgrade(url));
        QVERIFY(HttpsOnly::shouldUpgrade(QUrl(QStringLiteral("http://other.example/"))));
    }

    void warningPageIsEscaped()
    {
        const QString page = HttpsOnly::warningPage(QUrl(QStringLiteral("http://<b>evil</b>.example/\"x")));
        QVERIFY(!page.contains(QStringLiteral("<b>evil")));
        QVERIFY(page.contains(HttpsOnly::continueUrl().toString()));
    }
};

QTEST_MAIN(TestHttpsOnly)
#include "tst_httpsonly.moc"
