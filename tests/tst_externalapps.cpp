#include <QtTest>

#include "externalapps.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

using namespace spacepenguin;

class TestExternalApps : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void appLinksAreRecognised();
    void dangerousLinksAreNeverApps();
    void alwaysOpenIsPerSiteAndScheme();
    void flatpakRefIsRead();
    void flatpakRefRejectsOddValues();
    void systemHandlerIsFound();

private:
    QTemporaryDir m_bin;
};

void TestExternalApps::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguinTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_externalapps"));
    QSettings().clear();
}

void TestExternalApps::appLinksAreRecognised()
{
    QVERIFY(ExternalApps::isAppLink(QUrl(QStringLiteral("roblox-player:1+launchmode:play+gameinfo:abc"))));
    QVERIFY(ExternalApps::isAppLink(QUrl(QStringLiteral("discord://-/channels/@me"))));
    QVERIFY(ExternalApps::isAppLink(QUrl(QStringLiteral("vscode://auth/callback?code=123"))));
    QVERIFY(ExternalApps::isAppLink(QUrl(QStringLiteral("mailto:someone@example.com"))));
    QVERIFY(ExternalApps::isAppLink(QUrl(QStringLiteral("zoommtg://zoom.us/join?confno=1"))));
    // Web pages are not app links.
    QVERIFY(!ExternalApps::isAppLink(QUrl(QStringLiteral("https://www.roblox.com/"))));
    QVERIFY(!ExternalApps::isAppLink(QUrl(QStringLiteral("http://example.com/"))));
    QVERIFY(!ExternalApps::isAppLink(QUrl(QStringLiteral("wss://example.com/socket"))));
    // Qt refuses Discord's "-" host; the link is read back as the page wrote it.
    const QUrl discord(QStringLiteral("discord://-/channels/@me"));
    QCOMPARE(ExternalApps::linkText(discord), QStringLiteral("discord://-/channels/@me"));
    QCOMPARE(ExternalApps::linkText(QUrl(QStringLiteral("vscode://auth/callback?code=123"))),
             QStringLiteral("vscode://auth/callback?code=123"));
    QVERIFY(ExternalApps::isRoblox(QStringLiteral("roblox-player")));
    QVERIFY(!ExternalApps::isRoblox(QStringLiteral("discord")));
    QCOMPARE(ExternalApps::siteOf(QUrl(QStringLiteral("https://WWW.Roblox.com/games/1"))),
             QStringLiteral("www.roblox.com"));
}

void TestExternalApps::dangerousLinksAreNeverApps()
{
    for (const char *link : {"javascript:alert(1)", "file:///etc/passwd", "data:text/html,hi", "vbscript:x",
                             "ms-msdt:/id PCWDiagnostic", "search-ms:query=x", "shell:startup",
                             "ms-officecmd:{}", "view-source:https://example.com", "sp://logins/",
                             "about:blank", "blob:https://example.com/1", "ms-appinstaller:?source=x"}) {
        QVERIFY2(!ExternalApps::isAppLink(QUrl(QString::fromLatin1(link))), link);
    }
    QVERIFY(ExternalApps::isBlocked(QStringLiteral("JavaScript")));
    QVERIFY(!ExternalApps::open(QUrl(QStringLiteral("file:///etc/passwd"))));
}

void TestExternalApps::alwaysOpenIsPerSiteAndScheme()
{
    QVERIFY(!ExternalApps::isAlwaysAllowed(QStringLiteral("www.roblox.com"), QStringLiteral("roblox-player")));
    ExternalApps::setAlwaysAllowed(QStringLiteral("www.roblox.com"), QStringLiteral("Roblox-Player"));
    QVERIFY(ExternalApps::isAlwaysAllowed(QStringLiteral("www.roblox.com"), QStringLiteral("roblox-player")));
    // Not another site, not another app.
    QVERIFY(!ExternalApps::isAlwaysAllowed(QStringLiteral("evil.example"), QStringLiteral("roblox-player")));
    QVERIFY(!ExternalApps::isAlwaysAllowed(QStringLiteral("www.roblox.com"), QStringLiteral("discord")));
    // Nothing for pages without a site.
    ExternalApps::setAlwaysAllowed(QString(), QStringLiteral("discord"));
    QVERIFY(!ExternalApps::isAlwaysAllowed(QString(), QStringLiteral("discord")));
    // Stored once.
    ExternalApps::setAlwaysAllowed(QStringLiteral("www.roblox.com"), QStringLiteral("roblox-player"));
    QCOMPARE(QSettings().value(QStringLiteral("externalApps/alwaysOpen")).toStringList().size(), 1);
}

void TestExternalApps::flatpakRefIsRead()
{
    const QByteArray file = "[Flatpak Ref]\n"
                            "Name=org.vinegarhq.Sober\n"
                            "Branch=stable\n"
                            "Title=Sober\n"
                            "Url=https://dl.flathub.org/repo/\n"
                            "IsRuntime=false\n"
                            "GPGKey=mQINBFlD2sABEADsiUZUO…\n";
    const ExternalApps::FlatpakRef ref = ExternalApps::parseFlatpakRef(file);
    QVERIFY(ref.isValid());
    QCOMPARE(ref.name, QStringLiteral("org.vinegarhq.Sober"));
    QCOMPARE(ref.title, QStringLiteral("Sober"));
    QCOMPARE(ref.branch, QStringLiteral("stable"));
    QCOMPARE(ref.repository.host(), QStringLiteral("dl.flathub.org"));
    QVERIFY(ExternalApps::isFlatpakRef(QStringLiteral("/home/me/Downloads/org.vinegarhq.Sober.flatpakref")));
    QVERIFY(!ExternalApps::isFlatpakRef(QStringLiteral("/home/me/Downloads/notes.txt")));
}

void TestExternalApps::flatpakRefRejectsOddValues()
{
    // Not an app ID.
    QVERIFY(!ExternalApps::parseFlatpakRef("[Flatpak Ref]\nName=--help\nUrl=https://dl.flathub.org/repo/\n").isValid());
    QVERIFY(!ExternalApps::parseFlatpakRef("[Flatpak Ref]\nName=app\nUrl=https://dl.flathub.org/repo/\n").isValid());
    // A repository that is not on the web.
    QVERIFY(!ExternalApps::parseFlatpakRef("[Flatpak Ref]\nName=org.example.App\nUrl=file:///tmp/repo\n").isValid());
    // Keys outside [Flatpak Ref] count for nothing.
    QVERIFY(!ExternalApps::parseFlatpakRef("[Other]\nName=org.example.App\nUrl=https://dl.flathub.org/repo/\n").isValid());
    QVERIFY(!ExternalApps::parseFlatpakRef(QByteArray()).isValid());
}

void TestExternalApps::systemHandlerIsFound()
{
#if defined(Q_OS_LINUX)
    // A stand-in xdg-mime: Sober opens roblox-player links, nothing opens discord ones.
    QVERIFY(m_bin.isValid());
    QFile script(m_bin.filePath(QStringLiteral("xdg-mime")));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\n[ \"$3\" = x-scheme-handler/roblox-player ] && echo org.vinegarhq.Sober.desktop\nexit 0\n");
    script.close();
    script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString applications = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QVERIFY(QDir().mkpath(applications));
    QFile desktop(applications + QStringLiteral("/org.vinegarhq.Sober.desktop"));
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write("[Desktop Entry]\nType=Application\nName=Sober\nExec=sober %u\n"
                  "MimeType=x-scheme-handler/roblox-player;\n[Desktop Action Other]\nName=Not this\n");
    desktop.close();
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(m_bin.path()) + ':' + path);

    QVERIFY(ExternalApps::hasHandler(QStringLiteral("roblox-player")));
    QCOMPARE(ExternalApps::appName(QStringLiteral("roblox-player")), QStringLiteral("Sober"));
    QVERIFY(!ExternalApps::hasHandler(QStringLiteral("discord")));
    // Nothing found on the system: a known name, or the scheme itself.
    QCOMPARE(ExternalApps::appName(QStringLiteral("discord")), QStringLiteral("Discord"));
    QVERIFY(ExternalApps::appName(QStringLiteral("someapp")).contains(QStringLiteral("someapp:")));

    qputenv("PATH", path);
    QFile::remove(desktop.fileName());
#else
    QVERIFY(ExternalApps::hasHandler(QStringLiteral("discord")));
    QCOMPARE(ExternalApps::appName(QStringLiteral("discord")), QStringLiteral("Discord"));
#endif
}

QTEST_GUILESS_MAIN(TestExternalApps)
#include "tst_externalapps.moc"
