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

#pragma once

#include <QString>

namespace inputleap {
namespace fork_gui {

/* Maps daemon log lines (the GUI already receives every line the spawned
 * input-leapc/input-leaps prints) to overlay notifications. Called from
 * MainWindow::updateFromLogLine — the same choke point upstream uses for
 * its connection-state parsing.
 */
void routeLogLineToOverlay(const QString& line);

} // namespace fork_gui
} // namespace inputleap
