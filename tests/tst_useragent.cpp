#include <QtTest>

#include "useragent.h"

using namespace spacepenguin;

class TestUserAgent : public QObject
{
    Q_OBJECT

private slots:
    void dropsTheEngineToken_data();
    void dropsTheEngineToken();
};

void TestUserAgent::dropsTheEngineToken_data()
{
    QTest::addColumn<QString>("in");
    QTest::addColumn<QString>("out");
    const QString chrome = QStringLiteral(
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");
    QTest::newRow("linux") << QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                                             "QtWebEngine/6.11.2 Chrome/134.0.0.0 Safari/537.36")
                           << chrome;
    QTest::newRow("already clean") << chrome << chrome;
    QTest::newRow("windows") << QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                                               "Gecko) QtWebEngine/6.8.3 Chrome/122.0.0.0 Safari/537.36")
                             << QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                                               "Gecko) Chrome/122.0.0.0 Safari/537.36");
}

void TestUserAgent::dropsTheEngineToken()
{
    QFETCH(QString, in);
    QFETCH(QString, out);
    QCOMPARE(UserAgent::withoutEngineToken(in), out);
}

QTEST_GUILESS_MAIN(TestUserAgent)
#include "tst_useragent.moc"
