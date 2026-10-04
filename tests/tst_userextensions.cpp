#include <QtTest>

#include "userextensions.h"

using namespace spacepenguin;

class TestUserScripts : public QObject
{
    Q_OBJECT

private slots:
    void parsesName();
    void parsesMatchPatterns();
    void defaultsWhenNoMetadata();
    void injectedSourceGuardsByHost();
    void injectedSourceRunsEverywhereWithoutPatterns();
    void injectedSourceKeepsBody();

    void installsScriptFromElsewhere();
    void installingInPlaceIsIdempotent();
    void rejectsNonJavaScript();
    void rejectsDuplicate();
    void rejectsMissingFile();
    void disablesAndEnables();
    void removesScript();
    void missingExtensionDirectoryRejectsAdds();
};

static QString writeScript(const QString &directory, const QString &name, const QString &body)
{
    QFile file(QDir(directory).filePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    file.write(body.toUtf8());
    file.close();
    return file.fileName();
}

void TestUserScripts::parsesName()
{
    const QString body = QStringLiteral("// @name Greyscale\n// @match *://example.com/*\n"
                                        "document.body.classList.add('grey');\n");
    const UserScript script = UserScripts::parse(QStringLiteral("/tmp/grey.js"), body);

    QCOMPARE(script.name, QStringLiteral("Greyscale"));
    QCOMPARE(script.id, QStringLiteral("grey.js"));
    QCOMPARE(script.matchPatterns, QStringList{QStringLiteral("*://example.com/*")});
    QVERIFY(script.enabled);
}

void TestUserScripts::parsesMatchPatterns()
{
    const QString body = QStringLiteral("// @match *://a.example/*, *://b.example/*\n");
    QCOMPARE(UserScripts::parseMatchPatterns(body),
             QStringList({QStringLiteral("*://a.example/*"), QStringLiteral("*://b.example/*")}));
    QCOMPARE(UserScripts::parseName(body, QStringLiteral("fallback.js")),
             QStringLiteral("fallback.js"));
}

void TestUserScripts::defaultsWhenNoMetadata()
{
    const UserScript script =
        UserScripts::parse(QStringLiteral("/tmp/plain.js"), QStringLiteral("console.log(1);"));

    QCOMPARE(script.name, QStringLiteral("plain.js"));
    QVERIFY(script.matchPatterns.isEmpty());
}

void TestUserScripts::injectedSourceGuardsByHost()
{
    UserScript script;
    script.id = QStringLiteral("x.js");
    script.name = QStringLiteral("x");
    script.matchPatterns = {QStringLiteral("*://example.com/*")};

    const QString source = UserScripts::buildInjectedSource(script, QStringLiteral("doThing();"));
    QVERIFY(source.contains(QStringLiteral("doThing();")));
    QVERIFY(source.contains(QStringLiteral("location.hostname")));
    QVERIFY(source.contains(QStringLiteral("example\\.com")));
    QVERIFY(source.contains(QStringLiteral("if (!spPatterns.test(spHost)) return;")));
}

void TestUserScripts::injectedSourceRunsEverywhereWithoutPatterns()
{
    UserScript script;
    script.id = QStringLiteral("x.js");
    script.name = QStringLiteral("x");

    const QString source = UserScripts::buildInjectedSource(script, QStringLiteral("doThing();"));
    QVERIFY(source.contains(QStringLiteral("new RegExp(.*)")));
}

void TestUserScripts::injectedSourceKeepsBody()
{
    UserScript script;
    script.id = QStringLiteral("x.js");
    script.name = QStringLiteral("x");

    const QString source = UserScripts::buildInjectedSource(
        script, QStringLiteral("window.spProbe = 'yes';\n"));
    QVERIFY(source.contains(QStringLiteral("window.spProbe = 'yes';")));
    QVERIFY(source.contains(QStringLiteral("catch (error)")));
}

void TestUserScripts::installsScriptFromElsewhere()
{
    QTemporaryDir extensions;
    QTemporaryDir downloads;
    QVERIFY(extensions.isValid() && downloads.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString body = QStringLiteral("// @name Probe\nwindow.spProbe = 'yes';\n");
    const QString source = writeScript(downloads.path(), QStringLiteral("probe.js"), body);
    QVERIFY(!source.isEmpty());

    QString error;
    QVERIFY2(scripts.addScript(source, &error), qPrintable(error));
    QCOMPARE(scripts.scripts().size(), 1);
    QCOMPARE(scripts.scripts().first().name, QStringLiteral("Probe"));
    QVERIFY(scripts.isEnabled(QStringLiteral("probe.js")));
    QVERIFY(QFile::exists(QDir(extensions.path()).filePath(QStringLiteral("probe.js"))));
    QVERIFY(QFile::exists(source));
}

void TestUserScripts::installingInPlaceIsIdempotent()
{
    QTemporaryDir extensions;
    QVERIFY(extensions.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString path = writeScript(extensions.path(), QStringLiteral("inplace.js"),
                                     QStringLiteral("window.i = 1;"));
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(scripts.addScript(path, &error), qPrintable(error));
    QVERIFY2(scripts.addScript(path, &error), qPrintable(error));
    QCOMPARE(scripts.scripts().size(), 1);
}

void TestUserScripts::rejectsNonJavaScript()
{
    QTemporaryDir extensions;
    QVERIFY(extensions.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString path = writeScript(extensions.path(), QStringLiteral("notascript.txt"),
                                     QStringLiteral("nope"));

    QString error;
    QVERIFY(!scripts.addScript(path, &error));
    QVERIFY(error.contains(QStringLiteral(".js")));
}

void TestUserScripts::rejectsDuplicate()
{
    QTemporaryDir extensions;
    QTemporaryDir downloads;
    QVERIFY(extensions.isValid() && downloads.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString body = QStringLiteral("window.x = 1;");
    const QString first = writeScript(downloads.path(), QStringLiteral("dup.js"), body);
    QVERIFY(!first.isEmpty());

    QString error;
    QVERIFY2(scripts.addScript(first, &error), qPrintable(error));

    const QString second = writeScript(downloads.path(), QStringLiteral("dup.js"), body);
    QVERIFY(!second.isEmpty());
    QVERIFY(!scripts.addScript(second, &error));
    QVERIFY(error.contains(QStringLiteral("already exists")));
}

void TestUserScripts::rejectsMissingFile()
{
    QTemporaryDir extensions;
    QVERIFY(extensions.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    QString error;
    QVERIFY(!scripts.addScript(QStringLiteral("/nonexistent/thing.js"), &error));
    QVERIFY(!error.isEmpty());
}

void TestUserScripts::disablesAndEnables()
{
    QTemporaryDir extensions;
    QVERIFY(extensions.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString path = writeScript(extensions.path(), QStringLiteral("toggle.js"),
                                     QStringLiteral("window.t = 1;"));
    QVERIFY(scripts.addScript(path));

    QVERIFY(scripts.setScriptEnabled(QStringLiteral("toggle.js"), false));
    QVERIFY(!scripts.isEnabled(QStringLiteral("toggle.js")));
    QVERIFY(scripts.setScriptEnabled(QStringLiteral("toggle.js"), true));
    QVERIFY(scripts.isEnabled(QStringLiteral("toggle.js")));
    QVERIFY(!scripts.setScriptEnabled(QStringLiteral("missing.js"), true));
    QVERIFY(!scripts.isEnabled(QStringLiteral("missing.js")));
}

void TestUserScripts::removesScript()
{
    QTemporaryDir extensions;
    QVERIFY(extensions.isValid());

    UserScripts scripts(nullptr);
    scripts.setDirectory(extensions.path());

    const QString path = writeScript(extensions.path(), QStringLiteral("gone.js"),
                                     QStringLiteral("window.g = 1;"));
    QVERIFY(scripts.addScript(path));
    QCOMPARE(scripts.scripts().size(), 1);

    QVERIFY(scripts.removeScript(QStringLiteral("gone.js")));
    QCOMPARE(scripts.scripts().size(), 0);
    QVERIFY(!QFile::exists(path));
    QVERIFY(!scripts.removeScript(QStringLiteral("gone.js")));
}

void TestUserScripts::missingExtensionDirectoryRejectsAdds()
{
    QTemporaryDir downloads;
    QVERIFY(downloads.isValid());

    UserScripts scripts(nullptr);
    QVERIFY(scripts.scripts().isEmpty());

    const QString path = writeScript(downloads.path(), QStringLiteral("x.js"),
                                     QStringLiteral("window.x = 1;"));
    QString error;
    QVERIFY(!scripts.addScript(path, &error));
    QVERIFY(error.contains(QStringLiteral("extension directory")));
}

QTEST_MAIN(TestUserScripts)
#include "tst_userextensions.moc"