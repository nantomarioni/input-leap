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

/* Why this exists (the "flicker" investigation):
 *
 * input-leapc / input-leaps transform themselves into background-only apps
 * (App::run) and run a Cocoa loop with no real UI (runCocoaApp). AppKit then
 * treats the process as auto-quittable: it registers a memory-pressure
 * handler that calls [NSApp terminate:] -> exit(0). In the field this showed
 * up as the client dying ~1s after each (re)connect while the Mac was under
 * memory pressure, the GUI restarting it, and the screen dim flickering on
 * every cycle. The exit tracer caught the stack:
 *   _callMemoryPressureHandlers
 *     -> _tryTransformingToBackgroundTypeForAutoQuit (block)
 *     -> _terminateOnMemoryPressureFromMainRunLoopPerformBlock
 *     -> -[NSApplication terminate:]
 *
 * AppKit only installs that handler while -[NSProcessInfo _isAutoQuittable]
 * is true, i.e. while the automatic-termination disable count is zero.
 * Bumping the count once (never balanced) keeps the daemon alive.
 */

#include "OSXDragSimulatorExtension.h"
#include "base/LogExtension.h"

#import <Foundation/Foundation.h>

namespace inputleap {

void fork_disableAutomaticTermination() {
    [[NSProcessInfo processInfo]
        disableAutomaticTermination:@"input-leap daemon must keep running"];
    FORK_LOG("automatic termination disabled (memory-pressure auto-quit off)");
}

} // namespace inputleap
