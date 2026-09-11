/* Lightweight fork logging utility.
 * Provides FORK_LOG(...) macro that writes timestamped lines to a per-user
 * file without depending on the project's core logging headers, plus quit-
 * reason tracing used to diagnose silent process exits.
 */
#pragma once

#include <cstdarg>

namespace inputleap {
namespace fork {

// Write a formatted debug line (timestamp + pid prefixed) to the fork log.
// Thread-safe.
void logDebug(const char* fmt, ...);

// Async-signal-safe: mark that a terminate/interrupt signal requested quit.
void noteSignalQuit();

// True when noteSignalQuit() was called.
bool quitBySignal();

// Mark that normal teardown ran (set at the end of the app main loop).
void noteCleanTeardown();

// Install an atexit tracer that writes the exit path to the fork log:
// distinguishes signal-initiated quits, silent exit() (e.g. Cocoa
// [NSApp terminate]), and normal teardown.
void installExitTracer(const char* process_tag);

} // namespace fork
} // namespace inputleap

#define FORK_LOG(...) inputleap::fork::logDebug(__VA_ARGS__)
