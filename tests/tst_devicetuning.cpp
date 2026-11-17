#include <QtTest>

#include "devicetuning.h"

using namespace spacepenguin;

class TestDeviceTuning : public QObject
{
    Q_OBJECT

private slots:
    void bigMachineIsLeftAlone();
    void schoolChromebookGetsLowMemoryAndSoftwareDrawing();
    void chromebookWithGpuKeepsIt();
    void overrideWins();
    void userFlagsWin();
    void unknownMemoryIsNotLowMemory();
    void linuxVideosAvoidGpuBuffers();
};

static DeviceTuning::Facts facts(qint64 memoryMiB, bool chromeOS = false, bool gpu = true)
{
    DeviceTuning::Facts facts;
    facts.memoryMiB = memoryMiB;
    facts.chromeOS = chromeOS;
    facts.gpuDevice = gpu;
    return facts;
}

void TestDeviceTuning::bigMachineIsLeftAlone()
{
    const auto plan = DeviceTuning::plan(facts(16 * 1024));
    QVERIFY(!plan.lowMemory);
    QVERIFY(plan.flags.isEmpty());
    QVERIFY(plan.adjustments.isEmpty());
    QCOMPARE(plan.device, QStringLiteral("16.0 GB memory"));
}

void TestDeviceTuning::schoolChromebookGetsLowMemoryAndSoftwareDrawing()
{
    // A 4 GB Chromebook: the Linux VM sees a little less than all of it.
    const auto plan = DeviceTuning::plan(facts(3700, true, false));
    QVERIFY(plan.lowMemory);
    QVERIFY(plan.flags.contains(QStringLiteral("--renderer-process-limit=3")));
    QVERIFY(plan.flags.contains(QStringLiteral("--enable-low-end-device-mode")));
    QVERIFY(plan.flags.contains(QStringLiteral("--disable-gpu")));
    QVERIFY(plan.device.startsWith(QStringLiteral("Chromebook")));
    QVERIFY(plan.adjustments.contains(QStringLiteral("low-memory")));
}

void TestDeviceTuning::chromebookWithGpuKeepsIt()
{
    const auto plan = DeviceTuning::plan(facts(7800, true, true));
    QVERIFY(!plan.lowMemory);
    QVERIFY(plan.flags.isEmpty());
    // Only ChromeOS gets the software fallback; desktops without a render node
    // already fall back by themselves.
    QVERIFY(DeviceTuning::plan(facts(7800, false, false)).flags.isEmpty());
}

void TestDeviceTuning::overrideWins()
{
    auto forcedOff = facts(2048);
    forcedOff.lowMemoryOverride = 0;
    QVERIFY(!DeviceTuning::plan(forcedOff).lowMemory);

    auto forcedOn = facts(32 * 1024);
    forcedOn.lowMemoryOverride = 1;
    QVERIFY(DeviceTuning::plan(forcedOn).lowMemory);
}

void TestDeviceTuning::userFlagsWin()
{
    auto withFlags = facts(3700, true, false);
    withFlags.userFlags = QStringLiteral("--renderer-process-limit=8 --ignore-gpu-blocklist");
    const auto plan = DeviceTuning::plan(withFlags);
    QVERIFY(!plan.flags.contains(QStringLiteral("--renderer-process-limit=3")));
    QVERIFY(plan.flags.contains(QStringLiteral("--enable-low-end-device-mode")));
    QVERIFY(plan.flags.contains(QStringLiteral("--disable-gpu")));
}

void TestDeviceTuning::unknownMemoryIsNotLowMemory()
{
    const auto plan = DeviceTuning::plan(facts(0));
    QVERIFY(!plan.lowMemory);
    QVERIFY(plan.device.isEmpty());
}

void TestDeviceTuning::linuxVideosAvoidGpuBuffers()
{
    // QtWebEngine aborts on Linux when a video frame arrives as a two-plane GBM
    // buffer, which closed the browser on YouTube.
    auto desktop = facts(16 * 1024);
    desktop.onLinux = true;
    const auto plan = DeviceTuning::plan(desktop);
    QVERIFY(plan.flags.contains(QStringLiteral("--disable-accelerated-video-decode")));
    QVERIFY(plan.flags.contains(QStringLiteral("--disable-gpu-memory-buffer-video-frames")));
    QVERIFY(!plan.lowMemory);
    // A switch the user set themselves is left to them.
    desktop.userFlags = QStringLiteral("--disable-accelerated-video-decode=0");
    QVERIFY(!DeviceTuning::plan(desktop).flags.contains(QStringLiteral("--disable-accelerated-video-decode")));
}

QTEST_GUILESS_MAIN(TestDeviceTuning)
#include "tst_devicetuning.moc"
