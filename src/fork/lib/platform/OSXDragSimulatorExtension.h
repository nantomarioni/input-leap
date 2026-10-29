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

/* OSXDragSimulatorExtension - free-function hooks for runCocoaApp()
 */

#pragma once

namespace inputleap {

// Called by runCocoaApp() before [NSApp run]. Opts the daemon out of AppKit
// automatic termination, which otherwise quits it (exit(0), no teardown)
// whenever the system reports memory pressure.
void fork_disableAutomaticTermination();

} // namespace inputleap
