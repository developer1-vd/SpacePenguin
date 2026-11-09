#include <QtTest>

#include "adblocker.h"
#include "filterlists.h"
#include "session.h"
#include "sitepermissions.h"

using namespace spacepenguin;

// The data SpacePenguin keeps for people: the open tabs and the crash marker, the
// filter lists they subscribe to, and how site permissions are named. (Saved
// logins are in tst_importer, web apps and Tor in tst_roles, the classroom in
// tst_classroom.)
class TestUserData : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void sessionRoundTrip();
    void sessionKeepsWithinLimits();
    void uncleanExitIsNoticed();
    void crashedTabsAreSetAside();

    void filterListsCombineAndFollowSubscriptions();
    void downloadsMustLookLikeFilterLists();

    void sitePermissionNames();

private:
    QTemporaryDir m_dir;
};

void TestUserData::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguinTests"));
    QCoreApplication::setApplicationName(QStringLiteral("userdata-%1").arg(QCoreApplication::applicationPid()));
    QSettings().clear();
    QVERIFY(m_dir.isValid());
}

void TestUserData::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    QFile::remove(settings.fileName());
}

void TestUserData::sessionRoundTrip()
{
    Session::Saved saved;
    saved.urls = {QStringLiteral("https://example.com/"), QStringLiteral("sp://start/"),
                  QStringLiteral("https://news.example/today")};
    saved.histories = {QByteArray("\x01\x02\x03", 3), QByteArray(), QByteArray("history")};
    saved.groups = {{QStringLiteral("Homework"), 4, true}};
    saved.tabGroups = {-1, 0, 0};
    saved.currentTab = 2;
    QSettings settings;
    Session::write(settings, saved);

    const Session::Saved read = Session::read(settings, 100);
    QCOMPARE(read.urls, saved.urls);
    QCOMPARE(read.histories, saved.histories);
    QCOMPARE(read.tabGroups, saved.tabGroups);
    QCOMPARE(read.groups.size(), 1);
    QCOMPARE(read.groups.first().title, QStringLiteral("Homework"));
    QCOMPARE(read.groups.first().color, 4);
    QVERIFY(read.groups.first().collapsed);
    QCOMPARE(read.currentTab, 2);
}

void TestUserData::sessionKeepsWithinLimits()
{
    QSettings settings;
    Session::Saved many;
    for (int i = 0; i < 10; ++i) {
        many.urls.append(QStringLiteral("https://example.com/%1").arg(i));
        many.histories.append(QByteArray());
        many.tabGroups.append(7); // a group that isn't there
    }
    many.currentTab = 9;
    Session::write(settings, many);

    // A cap on how many tabs come back, and the tab in front within them.
    const Session::Saved read = Session::read(settings, 4);
    QCOMPARE(read.urls.size(), 4);
    QCOMPARE(read.histories.size(), 4);
    QCOMPARE(read.currentTab, 3);
    for (int place : read.tabGroups)
        QCOMPARE(place, -1);

    // Nothing saved reads as nothing, whatever else is in the settings.
    settings.remove(Session::kGroup);
    QVERIFY(Session::read(settings, 100).isEmpty());
    QCOMPARE(Session::read(settings, 100).currentTab, 0);
}

void TestUserData::uncleanExitIsNoticed()
{
    QSettings settings;
    settings.remove(QStringLiteral("session/running"));
    QVERIFY(!Session::markRunning(settings)); // first start ever
    Session::markClosed(settings);
    QVERIFY(!Session::markRunning(settings)); // quit properly last time
    // Killed while running: markClosed never ran.
    QVERIFY(Session::markRunning(settings));
    Session::markClosed(settings);
}

void TestUserData::crashedTabsAreSetAside()
{
    QSettings settings;
    settings.remove(Session::kGroup);
    QVERIFY(!Session::setAsideForRestore(settings)); // nothing to set aside

    Session::Saved before;
    before.urls = {QStringLiteral("https://a.example/"), QStringLiteral("https://b.example/")};
    before.histories = {{}, {}};
    before.tabGroups = {-1, -1};
    before.currentTab = 1;
    Session::write(settings, before);
    QVERIFY(Session::setAsideForRestore(settings));

    // The new window's own tab is saved over the session; the crashed tabs stay aside.
    Session::Saved now;
    now.urls = {QStringLiteral("sp://start/")};
    now.histories = {{}};
    now.tabGroups = {-1};
    Session::write(settings, now);
    QCOMPARE(Session::read(settings, 100).urls, now.urls);
    const Session::Saved aside = Session::read(settings, 100, Session::kCrashedGroup);
    QCOMPARE(aside.urls, before.urls);
    QCOMPARE(aside.currentTab, 1);
}

void TestUserData::filterListsCombineAndFollowSubscriptions()
{
    // The person's own list, and two downloaded ones, fresh so nothing is fetched.
    const QString own = m_dir.filePath(QStringLiteral("filters.txt"));
    QFile file(own);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("||own-rule.example^\n");
    file.close();
    QDir(m_dir.path()).mkpath(QStringLiteral("lists"));
    const auto writeList = [this](const QString &id, const QByteArray &rules) {
        QFile list(m_dir.filePath(QStringLiteral("lists/%1.txt").arg(id)));
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write(rules);
    };
    writeList(QStringLiteral("easylist"), "||easylist-rule.example^\n");
    writeList(QStringLiteral("easyprivacy"), "||easyprivacy-rule.example^\n");

    FilterLists *lists = FilterLists::instance();
    QVERIFY(lists->isSubscribed(QStringLiteral("easylist"))); // on by default
    QVERIFY(!lists->isSubscribed(QStringLiteral("peterlowe")));
    QSignalSpy changed(lists, &FilterLists::changed);
    AdBlocker blocker;
    lists->setUserList(own, true);
    lists->attach(&blocker);
    QTRY_VERIFY(blocker.wouldBlock(QUrl(QStringLiteral("https://easylist-rule.example/ad.js"))));
    QVERIFY(blocker.wouldBlock(QUrl(QStringLiteral("https://own-rule.example/x"))));
    QVERIFY(blocker.wouldBlock(QUrl(QStringLiteral("https://easyprivacy-rule.example/t.js"))));
    QCOMPARE(lists->listFile(QStringLiteral("easylist")), m_dir.filePath(QStringLiteral("lists/easylist.txt")));
    QVERIFY(lists->updated(QStringLiteral("easylist")).isValid());
    QVERIFY(!lists->updated(QStringLiteral("ublock")).isValid());

    // Dropping a list stops its blocking at once, and the choice is kept.
    lists->setSubscribed(QStringLiteral("easyprivacy"), false);
    QTRY_VERIFY(!blocker.wouldBlock(QUrl(QStringLiteral("https://easyprivacy-rule.example/t.js"))));
    QVERIFY(blocker.wouldBlock(QUrl(QStringLiteral("https://easylist-rule.example/ad.js"))));
    QVERIFY(!lists->isSubscribed(QStringLiteral("easyprivacy")));
    lists->setSubscribed(QStringLiteral("easyprivacy"), true);
    QTRY_VERIFY(blocker.wouldBlock(QUrl(QStringLiteral("https://easyprivacy-rule.example/t.js"))));
}

void TestUserData::downloadsMustLookLikeFilterLists()
{
    QByteArray list = "[Adblock Plus 2.0]\n! Title: Test\n";
    for (int i = 0; i < 12; ++i)
        list += "||ads" + QByteArray::number(i) + ".example^\n";
    QVERIFY(FilterLists::looksLikeFilterList(list));
    // A captive portal's page or an error must not replace a working list.
    QVERIFY(!FilterLists::looksLikeFilterList("<!DOCTYPE html><html><body>Sign in to the Wi-Fi</body></html>"));
    QVERIFY(!FilterLists::looksLikeFilterList(QByteArray()));
    QVERIFY(!FilterLists::looksLikeFilterList("||one.example^\n||two.example^\n"));
}

void TestUserData::sitePermissionNames()
{
    QCOMPARE(SitePermissions::siteName(QUrl(QStringLiteral("https://maps.example.com"))),
             QStringLiteral("maps.example.com"));
    QCOMPARE(SitePermissions::siteName(QUrl(QStringLiteral("http://localhost:8080"))),
             QStringLiteral("localhost:8080"));
    using Type = QWebEnginePermission::PermissionType;
    for (Type type : {Type::MediaAudioCapture, Type::MediaVideoCapture, Type::Geolocation, Type::Notifications,
                      Type::ClipboardReadWrite, Type::DesktopVideoCapture}) {
        QVERIFY(!SitePermissions::name(type).isEmpty());
        QVERIFY(!SitePermissions::request(type).isEmpty());
    }
    QVERIFY(SitePermissions::stateName(QWebEnginePermission::State::Granted)
            != SitePermissions::stateName(QWebEnginePermission::State::Denied));
}

QTEST_MAIN(TestUserData)
#include "tst_userdata.moc"
