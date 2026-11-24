#include <QtTest>

#include "setupwizard.h"

using namespace spacepenguin;

class TestFirstRun : public QObject
{
    Q_OBJECT

private slots:
    void newInstallGetsTheWizard();
    void everyoneElseDoesNot();
};

void TestFirstRun::newInstallGetsTheWizard()
{
    QVERIFY(FirstRun::shouldShowSetup(true, false, false, "xcb", false));
    QVERIFY(FirstRun::shouldShowSetup(true, false, false, "wayland", false));
    QVERIFY(FirstRun::shouldShowSetup(true, false, false, "windows", false));
}

void TestFirstRun::everyoneElseDoesNot()
{
    // Existing settings: an existing user, or a second start.
    QVERIFY(!FirstRun::shouldShowSetup(false, false, false, "xcb", false));
    // Private and Tor windows, and installed web apps.
    QVERIFY(!FirstRun::shouldShowSetup(true, true, false, "xcb", false));
    QVERIFY(!FirstRun::shouldShowSetup(true, false, true, "xcb", false));
    // Nobody to answer it: no screen, or SPACEPENGUIN_NO_SETUP.
    QVERIFY(!FirstRun::shouldShowSetup(true, false, false, "offscreen", false));
    QVERIFY(!FirstRun::shouldShowSetup(true, false, false, "minimal", false));
    QVERIFY(!FirstRun::shouldShowSetup(true, false, false, "xcb", true));
}

QTEST_GUILESS_MAIN(TestFirstRun)
#include "tst_firstrun.moc"
