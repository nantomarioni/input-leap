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

#include "PrimaryClientExtension.h"
#include "server/PrimaryClient.h"
#include "inputleap/Screen.h"

namespace inputleap {

PrimaryClientExtension::PrimaryClientExtension() {}
PrimaryClientExtension::~PrimaryClientExtension() {}

PrimaryClient* PrimaryClientExtension::host() const {
    return static_cast<PrimaryClient*>(const_cast<PrimaryClientExtension*>(this));
}

void PrimaryClientExtension::fork_dimScreen(bool dim) {
    PrimaryClient* pc = host();
    if (!pc) return;

    inputleap::Screen* screen = pc->m_screen;
    if (!screen) return;

    screen->fork_dimScreen(dim);
}

} // namespace inputleap
