#include <QtTest>

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

    SingleInstance second(key);
    const QStringList arguments = {QStringLiteral("--private"), QStringLiteral("https://example.com/"),
                                   QStringLiteral("penguins ünïcode")};
    QVERIFY(second.sendToRunningInstance(arguments));

    QVERIFY(received.wait(5000));
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
