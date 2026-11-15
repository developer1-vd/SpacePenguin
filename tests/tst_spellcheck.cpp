#include <QtTest>

#include "spellcheck.h"

using namespace spacepenguin;

// Which dictionaries spell checking uses, and gathering them from many folders.
class TestSpellCheck : public QObject
{
    Q_OBJECT

private slots:
    void choosesThePersonsLanguages_data()
    {
        QTest::addColumn<QStringList>("ui");
        QTest::addColumn<QStringList>("available");
        QTest::addColumn<QStringList>("chosen");
        const QStringList some{QStringLiteral("en_GB"), QStringLiteral("en_US"), QStringLiteral("de_DE"),
                               QStringLiteral("fr")};
        QTest::newRow("exact") << QStringList{"en-US", "en"} << some << QStringList{"en_US"};
        QTest::newRow("same language") << QStringList{"en-AU"} << some << QStringList{"en_GB"};
        QTest::newRow("bare language") << QStringList{"de"} << some << QStringList{"de_DE"};
        QTest::newRow("several") << QStringList{"fr-CA", "de-AT", "en-GB"} << some
                                 << QStringList{"fr", "de_DE", "en_GB"};
        QTest::newRow("none installed") << QStringList{"en-US"} << QStringList{} << QStringList{};
        QTest::newRow("none for the language") << QStringList{"ja-JP"} << some << QStringList{};
        QTest::newRow("at most three") << QStringList{"en-US", "en-GB", "de", "fr"} << some
                                       << QStringList{"en_US", "en_GB", "de_DE"};
    }

    void choosesThePersonsLanguages()
    {
        QFETCH(QStringList, ui);
        QFETCH(QStringList, available);
        QFETCH(QStringList, chosen);
        QCOMPARE(SpellCheck::chooseLanguages(ui, available), chosen);
    }

    void gathersDictionariesIntoOneFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Like the Flatpak BaseApp: a folder per language.
        for (const QString &path : {QStringLiteral("locale/de/qtwebengine_dictionaries/de_DE.bdic"),
                                    QStringLiteral("locale/en/qtwebengine_dictionaries/en_US.bdic")}) {
            QVERIFY(QDir(dir.path()).mkpath(QFileInfo(dir.filePath(path)).path()));
            QFile file(dir.filePath(path));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("BDic");
        }
        qputenv("QTWEBENGINE_DICTIONARIES_PATH", QFile::encodeName(dir.filePath(QStringLiteral("locale"))));
        const QString gathered = dir.filePath(QStringLiteral("gathered"));
        QStringList languages = SpellCheck::prepare(gathered);
        languages.sort();
        QCOMPARE(languages, (QStringList{"de_DE", "en_US"}));
        QVERIFY(QFileInfo::exists(gathered + QStringLiteral("/de_DE.bdic")));
        QCOMPARE(qgetenv("QTWEBENGINE_DICTIONARIES_PATH"), QFile::encodeName(QDir(gathered).absolutePath()));
        // Again, with the environment now pointing at the gathered folder: no loop, same result.
        languages = SpellCheck::prepare(gathered);
        QVERIFY(languages.isEmpty() || languages.size() == 2);
    }

    void describesLanguages()
    {
        QCOMPARE(SpellCheck::describe({QStringLiteral("de_DE")}), QStringLiteral("Deutsch"));
        QCOMPARE(SpellCheck::describe({QStringLiteral("de_DE"), QStringLiteral("de_AT")}),
                 QStringLiteral("Deutsch, Österreichisches Deutsch"));
    }
};

QTEST_MAIN(TestSpellCheck)
#include "tst_spellcheck.moc"
