#include <QtTest>

#include "classroom.h"
#include "preferences.h"

using namespace spacepenguin;

class TestClassroom : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void codes();
    void joinCodes();
    void matching();
    void parsesHttp();
    void keepsAtMostAHundredSites();
    void teacherAndStudent();
    void bypasses();
    void cleanupTestCase();
};

void TestClassroom::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("SpacePenguinTests"));
    QCoreApplication::setApplicationName(QStringLiteral("classroom-%1").arg(QCoreApplication::applicationPid()));
    QSettings().clear();
}

void TestClassroom::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    QFile::remove(settings.fileName());
}

void TestClassroom::codes()
{
    const QString code = Classroom::makeCode();
    QCOMPARE(code.size(), 6);
    for (const QChar c : code)
        QVERIFY(!QStringLiteral("01OIL").contains(c));
}

void TestClassroom::joinCodes()
{
    QString code, host;
    quint16 port = Classroom::kHttpPort;
    QVERIFY(Classroom::parseJoinCode(QStringLiteral(" k7mp3x "), &code, &host, &port));
    QCOMPARE(code, QStringLiteral("K7MP3X"));
    QVERIFY(host.isEmpty());
    QVERIFY(Classroom::parseJoinCode(QStringLiteral("K7MP3X@10.0.0.5:47803"), &code, &host, &port));
    QCOMPARE(host, QStringLiteral("10.0.0.5"));
    QCOMPARE(port, quint16(47803));
    QVERIFY(!Classroom::parseJoinCode(QStringLiteral("K7MP"), &code, &host, &port));
    QVERIFY(!Classroom::parseJoinCode(QStringLiteral("K7MP3X@not an address"), &code, &host, &port));
    QVERIFY(!Classroom::parseJoinCode(QStringLiteral("K7MP3X@10.0.0.5:99999"), &code, &host, &port));
}

void TestClassroom::matching()
{
    const QList<BlockedSite> sites = {{QStringLiteral("youtube.com"), QStringLiteral("Videos after class")},
                                      {QStringLiteral("games.example"), QString()}};
    const BlockedSite *hit = Classroom::match(QUrl(QStringLiteral("https://m.youtube.com/watch")), sites);
    QVERIFY(hit);
    QCOMPARE(hit->reason, QStringLiteral("Videos after class"));
    QVERIFY(!Classroom::match(QUrl(QStringLiteral("https://example.com/")), sites));
    QVERIFY(!Classroom::match(QUrl(QStringLiteral("https://youtube.com/")), sites, {QStringLiteral("youtube.com")}));
    QVERIFY(!Classroom::match(QUrl(QStringLiteral("sp://start/")), sites));
}

void TestClassroom::parsesHttp()
{
    QByteArray method, target, body;
    QCOMPARE(Classroom::parseHttp("GET /v1/class?code=A HTTP/1.1\r\nHost: x\r\n", &method, &target, &body),
             Classroom::Parse::Incomplete);
    QCOMPARE(Classroom::parseHttp("GET /v1/class?code=A HTTP/1.1\r\nHost: x\r\n\r\n", &method, &target, &body),
             Classroom::Parse::Complete);
    QCOMPARE(method, QByteArray("GET"));
    QCOMPARE(target, QByteArray("/v1/class?code=A"));
    QCOMPARE(Classroom::parseHttp("POST /v1/request HTTP/1.1\r\nContent-Length: 5\r\n\r\nab", &method, &target, &body),
             Classroom::Parse::Incomplete);
    QCOMPARE(Classroom::parseHttp("POST /v1/request HTTP/1.1\r\ncontent-length: 5\r\n\r\nabcde", &method, &target, &body),
             Classroom::Parse::Complete);
    QCOMPARE(body, QByteArray("abcde"));
    QCOMPARE(Classroom::parseHttp("NONSENSE\r\n\r\n", &method, &target, &body), Classroom::Parse::Bad);
}

void TestClassroom::keepsAtMostAHundredSites()
{
    QList<BlockedSite> sites;
    for (int i = 0; i < 130; ++i)
        sites.append({QStringLiteral("site%1.example").arg(i), QString()});
    sites.prepend({QStringLiteral("https://www.Site0.example/path"), QString()}); // the same as site0
    QCOMPARE(Classroom::instance()->setBlockedSites(sites), Classroom::kMaxSites);
    QCOMPARE(Classroom::instance()->blockedSites().constFirst().site, QStringLiteral("site0.example"));
}

void TestClassroom::teacherAndStudent()
{
    // One process plays both parts, over this computer's own network.
    Classroom *classroom = Classroom::instance();
    Preferences::instance()->setRoles({QStringLiteral("student"), QStringLiteral("teacher")});
    classroom->start();
    classroom->setClassName(QStringLiteral("Year 7 Science"));
    classroom->setBlockedSites({{QStringLiteral("youtube.com"), QStringLiteral("Videos after class")},
                                {QStringLiteral("games.example"), QString()}});
    classroom->setClassOpen(true);
    QVERIFY(classroom->serving());

    QSignalSpy joined(classroom, &Classroom::joinFinished);
#ifdef Q_OS_LINUX
    // Found by its code alone: discovery answers on this computer too. (Other
    // systems' CI machines may keep broadcasts in; code@address is the same join.)
    classroom->join(classroom->code().toLower(), QStringLiteral("Sam"));
#else
    classroom->join(QStringLiteral("%1@127.0.0.1:%2").arg(classroom->code()).arg(classroom->port()),
                    QStringLiteral("Sam"));
#endif
    QVERIFY(joined.wait(8000));
    QVERIFY2(joined.first().at(0).toBool(), qPrintable(joined.first().at(1).toString()));
    QCOMPARE(classroom->joinedClassName(), QStringLiteral("Year 7 Science"));
    QCOMPARE(classroom->joinedSiteCount(), 2);
    QCOMPARE(classroom->students().size(), 1);
    QCOMPARE(classroom->students().constFirst().name, QStringLiteral("Sam"));

    QString site, reason;
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("https://www.youtube.com/watch?v=1")), &site, &reason));
    QCOMPARE(site, QStringLiteral("youtube.com"));
    QCOMPARE(reason, QStringLiteral("Videos after class"));
    QVERIFY(!classroom->blocks(QUrl(QStringLiteral("https://wikipedia.org/"))));

    // The student asks; the teacher hears it and allows it for this student.
    QSignalSpy received(classroom, &Classroom::requestReceived);
    QSignalSpy asked(classroom, &Classroom::askFinished);
    classroom->ask(QStringLiteral("youtube.com"), QStringLiteral("For the volcano video"));
    QVERIFY(asked.wait(5000));
    QVERIFY(asked.first().at(0).toBool());
    QCOMPARE(received.size(), 1);
    QCOMPARE(classroom->requests().size(), 1);
    QCOMPARE(classroom->requests().constFirst().note, QStringLiteral("For the volcano video"));
    QCOMPARE(classroom->requests().constFirst().student, QStringLiteral("Sam"));

    QSignalSpy answered(classroom, &Classroom::answerReceived);
    classroom->answer(classroom->requests().constFirst().id, Classroom::Answer::AllowStudent);
    QVERIFY(classroom->requests().isEmpty());
    QVERIFY(answered.wait(20000)); // the next poll brings it
    QCOMPARE(answered.first().at(0).toString(), QStringLiteral("youtube.com"));
    QVERIFY(answered.first().at(1).toBool());
    QVERIFY(!classroom->blocks(QUrl(QStringLiteral("https://www.youtube.com/watch?v=1"))));
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("https://games.example/"))));

    // The usual ways around a block are blocked too.
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("view-source:https://games.example/"))));
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("https://games-example.translate.goog/?_x_tr_sl=auto"))));
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("https://web.archive.org/web/2024/https://games.example/"))));
    QVERIFY(classroom->blocks(QUrl(QStringLiteral("https://www.croxyproxy.com/"))));
    // ... while the site Sam may use stays usable under its other names.
    QVERIFY(!classroom->blocks(QUrl(QStringLiteral("https://youtu.be/abc"))));

    // Leaving is the teacher's to allow.
    QSignalSpy leaveAsked(classroom, &Classroom::askFinished);
    classroom->askToLeave();
    QVERIFY(leaveAsked.wait(5000));
    QCOMPARE(classroom->requests().size(), 1);
    QVERIFY(classroom->requests().constFirst().leave);
    QSignalSpy released(classroom, &Classroom::released);
    classroom->answer(classroom->requests().constFirst().id, Classroom::Answer::AllowStudent);
    QVERIFY(released.wait(20000));
    QVERIFY(!classroom->inClass());
    QVERIFY(!classroom->blocks(QUrl(QStringLiteral("https://games.example/"))));
    QVERIFY(classroom->students().isEmpty());

    // Back in, until the teacher closes the class: then everyone is let go.
    joined.clear();
    classroom->join(QStringLiteral("%1@127.0.0.1:%2").arg(classroom->code()).arg(classroom->port()),
                    QStringLiteral("Sam"));
    QVERIFY(joined.wait(8000));
    QVERIFY(joined.last().at(0).toBool());
    QVERIFY(classroom->inClass());
    released.clear();
    classroom->setClassOpen(false);
    QVERIFY(classroom->serving()); // still there to tell its students
    QVERIFY(released.wait(20000));
    QVERIFY(!classroom->inClass());

    // A closed class takes no one in.
    joined.clear();
    classroom->join(QStringLiteral("%1@127.0.0.1:%2").arg(classroom->code()).arg(classroom->port()),
                    QStringLiteral("Alex"));
    QVERIFY(joined.wait(8000));
    QVERIFY(!joined.last().at(0).toBool());
}

void TestClassroom::bypasses()
{
    QCOMPARE(Classroom::unwrap(QUrl(QStringLiteral("view-source:https://example.com/a?b=1"))),
             QUrl(QStringLiteral("https://example.com/a?b=1")));
    QCOMPARE(Classroom::unwrap(QUrl(QStringLiteral("https://www-my--site-com.translate.goog/page"))).host(),
             QStringLiteral("www.my-site.com"));
    QCOMPARE(Classroom::unwrap(QUrl(QStringLiteral("https://translate.google.com/translate?sl=auto&u=https://example.com/x"))).host(),
             QStringLiteral("example.com"));
    QCOMPARE(Classroom::unwrap(QUrl(QStringLiteral("https://web.archive.org/web/20240101000000/https://example.com/x"))).host(),
             QStringLiteral("example.com"));
    QVERIFY(!Classroom::unwrap(QUrl(QStringLiteral("https://example.com/"))).isValid());

    QVERIFY(Classroom::aliases(QStringLiteral("youtube.com")).contains(QStringLiteral("youtu.be")));
    QVERIFY(Classroom::aliases(QStringLiteral("twitter.com")).contains(QStringLiteral("x.com")));
    QVERIFY(Classroom::aliases(QStringLiteral("example.com")).isEmpty());

    QVERIFY(Classroom::isBypassSite(QUrl(QStringLiteral("https://www.proxysite.com/"))));
    QVERIFY(Classroom::isBypassSite(QUrl(QStringLiteral("https://archive.ph/abc"))));
    QVERIFY(!Classroom::isBypassSite(QUrl(QStringLiteral("https://en.wikipedia.org/"))));

    // A blocked site's other names are blocked with it.
    const QList<BlockedSite> sites = {{QStringLiteral("youtube.com"), QString()}};
    QVERIFY(Classroom::match(QUrl(QStringLiteral("https://youtu.be/abc")), sites));
    QVERIFY(Classroom::match(QUrl(QStringLiteral("https://www.youtube-nocookie.com/embed/abc")), sites));
}

QTEST_GUILESS_MAIN(TestClassroom)
#include "tst_classroom.moc"
