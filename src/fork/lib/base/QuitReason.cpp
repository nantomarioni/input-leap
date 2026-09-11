/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
    Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Quit-reason tracing.
 *
 * The flicker investigation showed the client process exiting with code 0
 * with NO teardown logs ("stopped client" never printed) — meaning the
 * process left via a path that bypasses mainLoop: a terminate signal, or
 * (on macOS) something ending the Cocoa loop / calling exit() directly.
 * This tracer makes every exit path leave a trace in the fork log:
 *
 *  - signal quits: EventQueue's interrupt handler calls noteSignalQuit()
 *    (async-signal-safe: just an atomic store);
 *  - any exit(): the atexit hook below logs the reason with a marker for
 *    whether normal teardown ran (mainLoop sets it via noteCleanTeardown).
 */

#include "base/LogExtension.h"

#include <atomic>
#include <cstdlib>

namespace inputleap {
namespace fork {

namespace {
std::atomic<bool> s_signal_quit{false};
std::atomic<bool> s_clean_teardown{false};
const char* s_tag = "process";
} // namespace

void noteSignalQuit() {
    // Async-signal-safe: atomic store only. The atexit tracer does the
    // actual logging outside signal context.
    s_signal_quit.store(true);
}

bool quitBySignal() {
    return s_signal_quit.load();
}

void noteCleanTeardown() {
    s_clean_teardown.store(true);
}

static void exit_tracer() {
    if (s_signal_quit.load()) {
        logDebug("%s exit: terminate/interrupt signal received (SIGTERM/SIGINT)", s_tag);
    }
    else if (s_clean_teardown.load()) {
        logDebug("%s exit: normal teardown", s_tag);
    }
    else {
        logDebug("%s exit: exit() WITHOUT teardown — direct quit "
                 "(e.g. Cocoa [NSApp terminate] / Apple Event / library exit call)", s_tag);
    }
}

void installExitTracer(const char* process_tag) {
    static bool installed = false;
    if (installed) return;
    installed = true;
    if (process_tag) s_tag = process_tag;
    logDebug("%s start: exit tracer installed", s_tag);
    std::atexit(&exit_tracer);
}

} // namespace fork
} // namespace inputleap
