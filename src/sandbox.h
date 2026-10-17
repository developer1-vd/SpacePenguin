#pragma once

#include <QString>

namespace spacepenguin {

/**
 * Chromium's renderer sandbox on Linux needs unprivileged user namespaces. Ubuntu
 * 23.10 and later block them with AppArmor for programs without a profile that
 * allows it, and Chromium then aborts at start-up ("No usable sandbox"). Rather
 * than crash, SpacePenguin checks first and, when the sandbox cannot work, starts
 * without it and says so in every window and in about:version.
 */
class Sandbox
{
public:
    /** Call before QApplication exists. May set QTWEBENGINE_DISABLE_SANDBOX. */
    static void prepare();

    static bool isEnabled();
    /** Why the sandbox is off, for people; empty when it is on. */
    static QString disabledReason();
    /** What to do about it, for people; empty when nothing can be done. */
    static QString fix();
};

}
