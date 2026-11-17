#include <QtTest>

#include "languages.h"

using namespace spacepenguin;

class TestLanguages : public QObject
{
    Q_OBJECT

private slots:
    void tagsAreNormalized();
    void regionalLanguagesAddThePlainOne();
    void emptyChoiceFollowsTheSystem();
    void namesAreTheLanguagesOwn();
    void everyLanguageCanBePicked();
};

void TestLanguages::tagsAreNormalized()
{
    QCOMPARE(Languages::normalizeTag(QStringLiteral("pt_br")), QStringLiteral("pt-BR"));
    QCOMPARE(Languages::normalizeTag(QStringLiteral(" ZH-hant-tw ")), QStringLiteral("zh-Hant-TW"));
    QCOMPARE(Languages::normalizeTag(QStringLiteral("es-419")), QStringLiteral("es-419"));
    QVERIFY(Languages::normalizeTag(QStringLiteral("en;q=0.9")).isEmpty());
    QVERIFY(Languages::normalizeTag(QStringLiteral("x")).isEmpty());
    QVERIFY(Languages::normalizeTag(QStringLiteral("en\r\nX-Evil: 1")).isEmpty());
}

void TestLanguages::regionalLanguagesAddThePlainOne()
{
    QCOMPARE(Languages::websiteLanguages({QStringLiteral("fr-CH"), QStringLiteral("en")}),
             (QStringList{QStringLiteral("fr-CH"), QStringLiteral("fr"), QStringLiteral("en")}));
    // The plain one comes after the last regional form of the language.
    QCOMPARE(Languages::websiteLanguages({QStringLiteral("en-GB"), QStringLiteral("en-US"), QStringLiteral("de")}),
             (QStringList{QStringLiteral("en-GB"), QStringLiteral("en-US"), QStringLiteral("en"), QStringLiteral("de")}));
    QCOMPARE(Languages::acceptLanguage({QStringLiteral("pt_BR")}), QStringLiteral("pt-BR,pt"));
    // Junk is dropped, never passed on into a request header.
    QCOMPARE(Languages::acceptLanguage({QStringLiteral("de\r\nX: y"), QStringLiteral("it")}), QStringLiteral("it"));
}

void TestLanguages::emptyChoiceFollowsTheSystem()
{
    const QStringList system = Languages::systemLanguages();
    QVERIFY(!system.isEmpty());
    QCOMPARE(Languages::websiteLanguages({}).first(), system.first());
}

void TestLanguages::namesAreTheLanguagesOwn()
{
    QCOMPARE(Languages::nativeName(QStringLiteral("de")), QStringLiteral("Deutsch"));
    QVERIFY(Languages::nativeName(QStringLiteral("pt-BR")).startsWith(QStringLiteral("Português")));
    QVERIFY(Languages::nativeName(QStringLiteral("pt-BR")).contains(QStringLiteral("Brasil")));
    // Where the name already says the region, it is not said twice.
    QCOMPARE(Languages::nativeName(QStringLiteral("fr-CH")).count(QStringLiteral("uisse")), 1);
    QVERIFY(Languages::nativeName(QStringLiteral("de-CH")).contains(QStringLiteral("Schweiz")));
}

void TestLanguages::everyLanguageCanBePicked()
{
    const QList<Languages::Language> all = Languages::allLanguages();
    QVERIFY(all.size() > 100);
    QStringList tags;
    for (const Languages::Language &language : all)
        tags << language.tag;
    for (const char *tag : {"en", "en-US", "hi", "sw", "ar-EG", "ja", "pt-BR", "yo"})
        QVERIFY2(tags.contains(QLatin1String(tag)), tag);
    QVERIFY(Languages::appLanguages().contains(QStringLiteral("en")));
}

QTEST_GUILESS_MAIN(TestLanguages)
#include "tst_languages.moc"
