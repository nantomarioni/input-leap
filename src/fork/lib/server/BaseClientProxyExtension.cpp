#/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
#    Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)
#
#    This package is free software; you can redistribute it and/or
#    modify it under the terms of the GNU General Public License
#    found in the file LICENSE that should have accompanied this file.
#
#    This package is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#*/

#include "BaseClientProxyExtension.h"
#include "server/BaseClientProxy.h"
#include "server/ClientProxy.h"
#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "server/IClientConnection.h"
#include "server/Server.h"
#include "server/ClientProxy1_6.h"
#include "base/Event.h"
#include "base/Log.h"
#include "server/PrimaryClient.h"
#include "inputleap/Screen.h"

#include "base/LogExtension.h"

namespace inputleap {

BaseClientProxyExtension::BaseClientProxyExtension() {}
BaseClientProxyExtension::~BaseClientProxyExtension() {
    // NOTE: deliberately no fork_dimScreen(false) here. During base-class
    // destruction the dynamic_casts in fork_dimScreen resolve to null (the
    // derived ClientProxy/PrimaryClient parts are already destroyed), so the
    // call was dead code — and had it worked, it would write kMsgCDimScreen
    // to the stream of a connection that is being torn down precisely
    // because it died (exception in a destructor => std::terminate).
    // Undim-on-disconnect is handled by Server::switchScreen ->
    // fork_dimScreenAll() when the server jumps back to the primary.
}

BaseClientProxy* BaseClientProxyExtension::host() const {
    return static_cast<BaseClientProxy*>(const_cast<BaseClientProxyExtension*>(this));
}

void BaseClientProxyExtension::fork_dimScreen(bool dim) {
    BaseClientProxy* client = host();
    if (!client) return;

    // Guess we are dealing with a remote client
    ClientProxy* cp = dynamic_cast<ClientProxy*>(client);
    if (cp) {
        IClientConnection& conn = cp->get_conn();
        IStream* stream = conn.get_stream();
        if (!stream) return;
    
        ProtocolUtil::writef(stream, kMsgCDimScreen, dim ? 1 : 0);
        return;
    }

    // We are dealing with a local client otherwise
    PrimaryClient* pc = dynamic_cast<PrimaryClient*>(client);
    if (pc) {
        pc->m_screen->fork_dimScreen(dim);
    }
}

bool BaseClientProxyExtension::fork_parseMessage(const std::uint8_t* code) {
    if (memcmp(code, kMsgDUndimRequest, 4) == 0) {
        return fork_recvUndimRequest();
    }
    return false;
}

bool BaseClientProxyExtension::fork_recvUndimRequest() {
    BaseClientProxy* base = host();
    if (!base) return false;

    ClientProxy1_6* client = dynamic_cast<ClientProxy1_6*>(base);
    if (!client) return false;

    LOG_NOTE("fork: client \"%s\" reports local input while dimmed - switching to it",
             client->getName().c_str());

    // Post via ServerExtension: Server's SERVER_SWITCH_TO_SCREEN handler is
    // registered on the input filter's event target, not the client's (the
    // previous direct add_event with client->get_event_target() was silently
    // dropped by the event queue - nobody listened on that target).
    client->getServer()->fork_switchToScreen(client->getName());
    return true;
}

} // namespace inputleap
