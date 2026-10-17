#include "devicetuning.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QLocale>

#ifdef Q_OS_UNIX
#    include <unistd.h>
#endif

namespace spacepenguin {
namespace {

DeviceTuning::Plan s_plan;

#ifdef Q_OS_LINUX
/** Crostini marks its containers with these; neither exists anywhere else. */
bool isChromeOSContainer()
{
    return QFile::exists(QStringLiteral("/dev/.cros_milestone"))
        || QDir(QStringLiteral("/opt/google/cros-containers")).exists();
}

bool hasGpuRenderNode()
{
    return !QDir(QStringLiteral("/dev/dri"))
                .entryList({QStringLiteral("renderD*")}, QDir::System)
                .isEmpty();
}
#endif

qint64 physicalMemoryMiB()
{
#if defined(Q_OS_UNIX) && defined(_SC_PHYS_PAGES)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pages > 0 && pageSize > 0)
        return qint64(pages) * pageSize / (1024 * 1024);
#endif
    return 0;
}

/** The switch name of "--name=value" or "--name". */
QString switchName(const QString &flag)
{
    return flag.section(QLatin1Char('='), 0, 0);
}

} // namespace

DeviceTuning::Plan DeviceTuning::plan(const Facts &facts)
{
    Plan plan;

    QStringList found;
    if (facts.chromeOS)
        found << QStringLiteral("Chromebook (Linux on ChromeOS)");
    if (facts.memoryMiB > 0) {
        found << QStringLiteral("%1 GB memory")
                     .arg(QLocale::c().toString(facts.memoryMiB / 1024.0, 'f', 1));
    }
    plan.device = found.join(QStringLiteral(", "));

    plan.lowMemory = facts.lowMemoryOverride >= 0
        ? facts.lowMemoryOverride == 1
        : facts.memoryMiB > 0 && facts.memoryMiB <= LowMemoryLimitMiB;

    QStringList wanted;
    QStringList why;
    if (plan.lowMemory) {
        // A handful of renderer processes shared between tabs instead of one per
        // site, and Chromium's own low-end-device mode, which trims caches and
        // backgrounded tabs. This is what Chrome itself does on small Chromebooks.
        wanted << QStringLiteral("--renderer-process-limit=3")
               << QStringLiteral("--enable-low-end-device-mode");
        why << QStringLiteral("low-memory mode");
    }
    if (facts.chromeOS && !facts.gpuDevice) {
        // Without a render node Chromium's GPU process can only fail and restart,
        // which shows as black or blank pages. Software drawing works everywhere.
        wanted << QStringLiteral("--disable-gpu");
        why << QStringLiteral("software drawing (no GPU acceleration for Linux apps)");
    }

    // Whatever the user passed wins: skip any switch they set themselves.
    QStringList userSwitches;
    for (const QString &flag : facts.userFlags.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        userSwitches << switchName(flag);
    for (const QString &flag : std::as_const(wanted)) {
        if (!userSwitches.contains(switchName(flag)))
            plan.flags << flag;
    }

    if (!plan.flags.isEmpty())
        plan.adjustments = why.join(QStringLiteral(", "));
    return plan;
}

void DeviceTuning::prepare()
{
    Facts facts;
#ifdef Q_OS_LINUX
    facts.chromeOS = isChromeOSContainer();
    facts.gpuDevice = hasGpuRenderNode();
#endif
    facts.memoryMiB = physicalMemoryMiB();
    const QByteArray override = qgetenv("SPACEPENGUIN_LOW_MEMORY");
    if (!override.isEmpty())
        facts.lowMemoryOverride = override == "0" ? 0 : 1;
    facts.userFlags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");

    s_plan = plan(facts);
    if (!s_plan.flags.isEmpty()) {
        QStringList flags = facts.userFlags.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        flags << s_plan.flags;
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.join(QLatin1Char(' ')).toLocal8Bit());
    }
}

const DeviceTuning::Plan &DeviceTuning::current()
{
    return s_plan;
}

}
