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

#include "ClientExtension.h"
#include "client/Client.h"
#include "inputleap/Screen.h"

namespace inputleap {

ClientExtension::ClientExtension(): m_isDimmed(false) {}
ClientExtension::~ClientExtension() {}

Client* ClientExtension::host() const {
    return static_cast<Client*>(const_cast<ClientExtension*>(this));
}

void ClientExtension::fork_dimScreen(bool dim) {
    Client* c = host();
    if (!c) return;
    if (c->m_screen) c->m_screen->fork_dimScreen(dim);
    m_isDimmed = dim;
}

} // namespace inputleap
