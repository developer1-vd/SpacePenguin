#include "startuptrace.h"

#include <QElapsedTimer>
#include <QtGlobal>

#include <cstdio>

namespace spacepenguin::StartupTrace {
namespace {

QElapsedTimer &clock()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer;
}

bool enabled()
{
    static const bool on = qEnvironmentVariableIntValue("SPACEPENGUIN_TRACE_STARTUP") != 0;
    return on;
}

} // namespace

void mark(const char *stage)
{
    if (!enabled())
        return;
    std::fprintf(stderr, "[startup] %6lld ms  %s\n", static_cast<long long>(clock().elapsed()), stage);
    std::fflush(stderr);
}

void markOnce(const char *stage)
{
    static bool done = false;
    if (done)
        return;
    done = true;
    mark(stage);
}

}
