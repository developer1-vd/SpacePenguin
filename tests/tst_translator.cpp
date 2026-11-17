#include <QtTest>

#include "translator.h"

using namespace spacepenguin;
using Service = Translator::Service;

class TestTranslator : public QObject
{
    Q_OBJECT

private slots:
    void targetLanguages();
    void googleTranslatesPages();
    void onlyWebPagesAreSent();
    void textTranslators();
    void longTextIsCut();
    void deeplKeepsTextInTheFragment();
    void translatorPagesAreKnown();
    void scriptsCarryTranslationsAsData();
};

void TestTranslator::targetLanguages()
{
    QCOMPARE(Translator::targetLanguage(QStringLiteral("pt-BR")), QStringLiteral("pt"));
    QCOMPARE(Translator::targetLanguage(QStringLiteral("zh-Hant-TW")), QStringLiteral("zh-TW"));
    QCOMPARE(Translator::targetLanguage(QStringLiteral("zh")), QStringLiteral("zh-CN"));
    QCOMPARE(Translator::targetLanguage(QString()), QStringLiteral("en"));
}

void TestTranslator::googleTranslatesPages()
{
    const QUrl page(QStringLiteral("https://example.fr/article?id=4&x=a b#top"));
    const QUrl translated = Translator::pageUrl(Service::Google, page, QStringLiteral("en"));
    QCOMPARE(translated.host(), QStringLiteral("translate.google.com"));
    const QUrlQuery query(translated);
    QCOMPARE(query.queryItemValue(QStringLiteral("tl")), QStringLiteral("en"));
    QCOMPARE(QUrl(query.queryItemValue(QStringLiteral("u"), QUrl::FullyDecoded)), page);
    QVERIFY(!Translator::pageUrl(Service::DeepL, page, QStringLiteral("en")).isValid());
    QVERIFY(Translator::translatesPages(Service::LibreTranslate));
    QVERIFY(!Translator::translatesPages(Service::Bing));
}

void TestTranslator::onlyWebPagesAreSent()
{
    // Local files, the start page and translations themselves never go out.
    for (const char *address : {"file:///home/me/notes.html", "sp://start/", "about:blank",
                                "https://www-example-com.translate.goog/"})
        QVERIFY2(!Translator::pageUrl(Service::Google, QUrl(QLatin1String(address)), QStringLiteral("en")).isValid(),
                 address);
}

void TestTranslator::textTranslators()
{
    const QString text = QStringLiteral("Bonjour & bienvenue ?");
    for (const Service service : {Service::Google, Service::Bing, Service::Yandex}) {
        const QUrl url = Translator::textUrl(service, text, QStringLiteral("en"));
        QVERIFY2(url.isValid() && url.scheme() == QLatin1String("https"), qPrintable(url.toString()));
        bool found = false;
        for (const auto &item : QUrlQuery(url).queryItems(QUrl::FullyDecoded))
            found = found || item.second == text;
        QVERIFY2(found, qPrintable(url.toString()));
    }
    const QUrl libre = Translator::textUrl(Service::LibreTranslate, text, QStringLiteral("de"),
                                           QUrl(QStringLiteral("https://lt.example.org/some/path")));
    QCOMPARE(libre.host(), QStringLiteral("lt.example.org"));
    QCOMPARE(QUrlQuery(libre).queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), text);
    QVERIFY(!Translator::textUrl(Service::LibreTranslate, text, QStringLiteral("de"), QUrl()).isValid());
    QVERIFY(!Translator::textUrl(Service::LibreTranslate, text, QStringLiteral("de"),
                                 QUrl(QStringLiteral("javascript:alert(1)"))).isValid());
}

void TestTranslator::longTextIsCut()
{
    const QString text = QStringLiteral("mot ").repeated(2000);
    const QUrl url = Translator::textUrl(Service::Bing, text, QStringLiteral("en"));
    QVERIFY(QUrlQuery(url).queryItemValue(QStringLiteral("text"), QUrl::FullyDecoded).size()
            <= Translator::textLimit(Service::Bing));
}

void TestTranslator::deeplKeepsTextInTheFragment()
{
    const QUrl url = Translator::textUrl(Service::DeepL, QStringLiteral("a/b"), QStringLiteral("en"));
    QCOMPARE(url.host(), QStringLiteral("www.deepl.com"));
    QVERIFY(url.query().isEmpty());
    QCOMPARE(url.fragment(QUrl::FullyDecoded), QStringLiteral("auto/en/a\\/b"));
}

void TestTranslator::translatorPagesAreKnown()
{
    QVERIFY(Translator::isTranslatorUrl(QUrl(QStringLiteral("https://translate.google.de/?text=x"))));
    QVERIFY(Translator::isTranslatorUrl(QUrl(QStringLiteral("https://news-example-com.translate.goog/a"))));
    QVERIFY(Translator::isTranslatorUrl(QUrl(QStringLiteral("https://www.bing.com/translator?text=x"))));
    QVERIFY(!Translator::isTranslatorUrl(QUrl(QStringLiteral("https://www.bing.com/search?q=x"))));
    QVERIFY(!Translator::isTranslatorUrl(QUrl(QStringLiteral("https://example.com/"))));
}

void TestTranslator::scriptsCarryTranslationsAsData()
{
    const QString script = Translator::applyScript({QStringLiteral("</script>\"'); alert(1); //")});
    // The translations are one JSON array literal: quotes are escaped, nothing breaks out.
    QVERIFY(script.contains(QStringLiteral("[\"</script>\\\"'); alert(1); //\"]")));
    QVERIFY(Translator::collectScript().contains(QStringLiteral("createTreeWalker")));
}

QTEST_GUILESS_MAIN(TestTranslator)
#include "tst_translator.moc"
