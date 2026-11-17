#include <QtTest>

#include "readaloud.h"

using namespace spacepenguin;

class TestReadAloud : public QObject
{
    Q_OBJECT

private slots:
    void shortTextIsOnePiece();
    void paragraphsAndSentencesSplit();
    void longSentencesAreCut();
    void commandsNeverUseAShell();
    void languageTagCannotEscapeTheScript();
};

void TestReadAloud::shortTextIsOnePiece()
{
    QCOMPARE(ReadAloud::chunks(QStringLiteral("  Hello,\n world.  ")), QStringList{QStringLiteral("Hello, world.")});
    QVERIFY(ReadAloud::chunks(QStringLiteral(" \n\n ")).isEmpty());
}

void TestReadAloud::paragraphsAndSentencesSplit()
{
    const QStringList pieces = ReadAloud::chunks(QStringLiteral("One.\n\nTwo is here.\n\nThree."));
    QCOMPARE(pieces, (QStringList{QStringLiteral("One."), QStringLiteral("Two is here."), QStringLiteral("Three.")}));
    // A long paragraph is split at sentence ends, each piece within the limit.
    QString paragraph;
    for (int i = 0; i < 40; ++i)
        paragraph += QStringLiteral("This is sentence number %1. ").arg(i);
    const QStringList split = ReadAloud::chunks(paragraph, 200);
    QVERIFY(split.size() > 1);
    for (const QString &piece : split) {
        QVERIFY(piece.size() <= 200);
        QVERIFY(piece.endsWith(QLatin1Char('.')));
    }
    QCOMPARE(split.join(QLatin1Char(' ')), paragraph.simplified());
}

void TestReadAloud::longSentencesAreCut()
{
    const QString words = QStringLiteral("word ").repeated(200);
    const QStringList pieces = ReadAloud::chunks(words, 100);
    QVERIFY(pieces.size() >= 10);
    for (const QString &piece : pieces)
        QVERIFY(piece.size() <= 100);
}

void TestReadAloud::commandsNeverUseAShell()
{
    using Engine = ReadAloud::Engine;
    for (const Engine engine : {Engine::MacSay, Engine::WindowsSpeech, Engine::SpeechDispatcher, Engine::ESpeak}) {
        const ReadAloud::Command command = ReadAloud::command(engine, 0, QStringLiteral("en-US"));
        QVERIFY(!command.program.isEmpty());
        QVERIFY(command.program != QLatin1String("sh") && command.program != QLatin1String("cmd"));
    }
    // spd-say gets the text as the argument after "--", so text starting with "-"
    // is not read as an option.
    const ReadAloud::Command spd = ReadAloud::command(Engine::SpeechDispatcher, 2, QStringLiteral("pt-BR"));
    QCOMPARE(spd.arguments.last(), QStringLiteral("--"));
    QVERIFY(spd.arguments.contains(QStringLiteral("pt")));
    QVERIFY(spd.arguments.contains(QStringLiteral("40")));
    QVERIFY(!spd.textOnStdin);
    QVERIFY(ReadAloud::command(Engine::MacSay, 0, QString()).textOnStdin);
    QCOMPARE(ReadAloud::command(Engine::None, 0, QString()).program, QString());
}

void TestReadAloud::languageTagCannotEscapeTheScript()
{
    const ReadAloud::Command command =
        ReadAloud::command(ReadAloud::Engine::WindowsSpeech, 0, QStringLiteral("en'; Remove-Item C:\\\\ -Recurse; '"));
    const QString script = command.arguments.last();
    QVERIFY(!script.contains(QStringLiteral("Remove-Item C")));
    QVERIFY(!script.contains(QStringLiteral("en';")));
}

QTEST_GUILESS_MAIN(TestReadAloud)
#include "tst_readaloud.moc"
