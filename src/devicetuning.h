#pragma once

#include <QString>
#include <QStringList>

namespace spacepenguin {

/**
 * Adjusts Chromium for the machine it starts on, before QtWebEngine starts.
 *
 * The case it exists for is the school Chromebook: SpacePenguin runs there in
 * ChromeOS's Linux environment (Crostini), a container in a VM that gets part of
 * a 4 GB machine and, on many models, no GPU acceleration. Chromium's defaults —
 * a renderer process per site, GPU rasterization — are made for bigger machines,
 * and on these they mean swapping, black windows or a crashed GPU process.
 */
class DeviceTuning
{
public:
    /** What start-up found out about the machine. */
    struct Facts {
        /** Running in ChromeOS's Linux environment. */
        bool chromeOS = false;
        /** Physical memory visible to SpacePenguin, in MiB; 0 when unknown. */
        qint64 memoryMiB = 0;
        /** A GPU render node (/dev/dri/renderD*) exists. */
        bool gpuDevice = true;
        /** SPACEPENGUIN_LOW_MEMORY: -1 unset, 0 forced off, 1 forced on. */
        int lowMemoryOverride = -1;
        /** QTWEBENGINE_CHROMIUM_FLAGS as the user set it. */
        QString userFlags;
    };

    /** What to do about it. */
    struct Plan {
        bool lowMemory = false;
        /** Chromium switches to add, without any the user already passed. */
        QStringList flags;
        /** For about:version, e.g. "Chromebook (Linux on ChromeOS), 3.7 GB memory". */
        QString device;
        /** For about:version: what was changed, empty when nothing was. */
        QString adjustments;
    };

    /** Machines with at most this much memory get the low-memory settings. */
    static constexpr qint64 LowMemoryLimitMiB = 4608;

    /** Looks at this machine and applies the plan. Call before QApplication exists. */
    static void prepare();

    /** The decision itself, separate from looking at the machine so it can be tested. */
    static Plan plan(const Facts &facts);

    /** The plan prepare() applied. */
    static const Plan &current();
};

}
