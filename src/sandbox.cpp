#include "sandbox.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>

#ifdef Q_OS_LINUX
#    include <sched.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace spacepenguin {
namespace {

bool s_enabled = true;
QString s_reason;
QString s_fix;

#ifdef Q_OS_LINUX
/**
 * Does in a throwaway child what Chromium's namespace sandbox does: a new user
 * namespace together with new PID and network namespaces. Under Ubuntu's AppArmor
 * restriction the user namespace is created but carries no capabilities, so the
 * other two fail, exactly as they would for Chromium.
 */
bool namespacesWork()
{
    const pid_t child = fork();
    if (child < 0)
        return true; // cannot tell; let Chromium decide
    if (child == 0)
        _exit(unshare(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET) == 0 ? 0 : 1);
    int status = 0;
    if (waitpid(child, &status, 0) != child)
        return true;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool appArmorRestrictsNamespaces()
{
    QFile file(QStringLiteral("/proc/sys/kernel/apparmor_restrict_unprivileged_userns"));
    return file.open(QIODevice::ReadOnly) && file.readAll().trimmed() == "1";
}
#endif

} // namespace

void Sandbox::prepare()
{
    if (qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX")
        && qEnvironmentVariable("QTWEBENGINE_DISABLE_SANDBOX") != QLatin1String("0")) {
        s_enabled = false;
        s_reason = QStringLiteral("it was turned off with QTWEBENGINE_DISABLE_SANDBOX");
        return;
    }

#ifdef Q_OS_LINUX
    if (geteuid() == 0) {
        s_enabled = false;
        s_reason = QStringLiteral("SpacePenguin is running as root, where Chromium's sandbox cannot work");
        s_fix = QStringLiteral("Run SpacePenguin as your normal user, not with sudo.");
    } else if (!namespacesWork()) {
        s_enabled = false;
        if (appArmorRestrictsNamespaces()) {
            s_reason = QStringLiteral("AppArmor on this system blocks the user namespaces "
                                      "Chromium's sandbox needs");
            s_fix = QStringLiteral("To fix it, run sudo packaging/linux/install-apparmor-profile.sh "
                                   "from the SpacePenguin source and restart SpacePenguin. The "
                                   ".deb and .rpm packages set this up themselves.");
        } else {
            s_reason = QStringLiteral("this system does not allow the user namespaces "
                                      "Chromium's sandbox needs");
            s_fix = QStringLiteral("Enable unprivileged user namespaces "
                                   "(sysctl kernel.unprivileged_userns_clone=1).");
        }
    }

    if (!s_enabled)
        qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
#endif
}

bool Sandbox::isEnabled()
{
    return s_enabled;
}

QString Sandbox::disabledReason()
{
    return s_reason;
}

QString Sandbox::fix()
{
    return s_fix;
}

}
