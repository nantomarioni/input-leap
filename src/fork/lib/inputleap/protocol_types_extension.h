
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

namespace inputleap {

// screen dimming:  primary -> secondary
// instructs the secondary screen to dim ($1 == 1) or restore ($1 == 0) its brightness.
// used for automatically dimming inactive screens when switching between computers.
extern const char*        kMsgCDimScreen;

// undim request:  secondary -> primary
// sent by secondary when local input is detected on a dimmed screen.
// requests the primary to undim this client.
extern const char*        kMsgDUndimRequest;

} // namespace inputleap
