#pragma once

namespace spacepenguin {

/**
 * Opt-in timing of start-up, for finding out why it is slow on a given machine:
 * with SPACEPENGUIN_TRACE_STARTUP=1 each stage prints the time since the process
 * started to stderr. Costs nothing when the variable is not set.
 */
namespace StartupTrace {

/** Prints "[startup] <ms> ms  <stage>" if tracing is on. */
void mark(const char *stage);

/** Like mark(), but only the first time it is called with any stage. */
void markOnce(const char *stage);

}

}
