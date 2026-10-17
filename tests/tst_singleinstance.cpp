#include <QtTest>

#include <atomic>

#include "singleinstance.h"

using namespace spacepenguin;

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void nothingRunning();
    void handsArgumentsToRunningInstance();
    void profilesAreSeparate();
};

static QString uniqueKey()
{
    return QStringLiteral("test-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QRandomGenerator::global()->generate());
}

void TestSingleInstance::nothingRunning()
{
    SingleInstance instance(uniqueKey());
    QVERIFY(!instance.sendToRunningInstance({QStringLiteral("example.com")}));
}

void TestSingleInstance::handsArgumentsToRunningInstance()
{
    const QString key = uniqueKey();
    SingleInstance running(key);
    QVERIFY(running.listen());
    QSignalSpy received(&running, &SingleInstance::argumentsReceived);

    const QStringList arguments = {QStringLiteral("--private"), QStringLiteral("https://example.com/"),
                                   QStringLiteral("penguins ünïcode")};

    // A second launch is another process; send from another thread so this one's
    // event loop can accept, as it would in the running browser. (A Windows named
    // pipe write waits for the reader, so a same-thread send would deadlock.)
    std::atomic<bool> sent = false;
    QThread *sender = QThread::create([&key, &arguments, &sent] {
        SingleInstance second(key);
        sent = second.sendToRunningInstance(arguments);
    });
    sender->start();

    QVERIFY(received.wait(5000));
    QVERIFY(sender->wait(5000));
    delete sender;
    QVERIFY(sent);
    QCOMPARE(received.first().first().toStringList(), arguments);
}

void TestSingleInstance::profilesAreSeparate()
{
    SingleInstance a(QStringLiteral("/home/a/profile"));
    SingleInstance b(QStringLiteral("/home/b/profile"));
    QVERIFY(a.serverName() != b.serverName());
    QVERIFY(!a.serverName().contains(QLatin1Char('/')));
    QVERIFY(!a.serverName().contains(QLatin1Char('\\')));
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
