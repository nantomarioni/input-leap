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

#include "ServerProxyExtension.h"
#include "client/ServerProxy.h"
#include "client/Client.h"
#include "inputleap/protocol_types.h"
#include "inputleap/ProtocolUtil.h"

namespace inputleap {

ServerProxyExtension::ServerProxyExtension() {}
ServerProxyExtension::~ServerProxyExtension() {}

ServerProxy* ServerProxyExtension::host() const {
    return static_cast<ServerProxy*>(const_cast<ServerProxyExtension*>(this));
}

bool ServerProxyExtension::fork_parseMessage(const std::uint8_t* code) {
    if (memcmp(code, kMsgCDimScreen, 4) == 0) {
        fork_dimScreen();
        return true;
    }
    return false;
}

void ServerProxyExtension::fork_dimScreen() {
    ServerProxy* sp = host();
    // parse
    std::int8_t dim;
    ProtocolUtil::readf(sp->m_stream, kMsgCDimScreen + 4, &dim);

    // forward as a command so forked client extensions can handle it
    sp->m_client->fork_dimScreen(dim != 0);
}

} // namespace inputleap
