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

#include "ServerExtension.h"
#include "server/Server.h"
#include "server/BaseClientProxy.h"
#include "base/Log.h"
#include "base/Event.h"
#include "base/IEventQueue.h"

namespace inputleap {

ServerExtension::ServerExtension() {}
ServerExtension::~ServerExtension() {}

Server* ServerExtension::host() const {
    return static_cast<Server*>(const_cast<ServerExtension*>(this));
}

void ServerExtension::fork_dimScreenAll() {
    Server* srv = host();
    for (const auto& clientPair : srv->m_clients) {
        BaseClientProxy* clientProxy = clientPair.second;
        if (clientProxy == nullptr) continue;
        if (clientProxy == srv->m_active) {
            clientProxy->fork_dimScreen(false);
        } else {
            clientProxy->fork_dimScreen(true);
        }
    }
}

void ServerExtension::fork_clientAdopted(BaseClientProxy* client) {
    Server* srv = host();

    // Initial screen dimming: new clients should be dimmed if they are not
    // the active screen.
    if (client != srv->m_active) {
        fork_dimScreenAll();
    }

    // Flap detection: a healthy client adopts once; a connect/drop loop
    // adopts every second or two. Warn loudly (main server log) when the
    // same name reconnects 4+ times inside a minute.
    const auto now = std::chrono::steady_clock::now();
    const auto window = std::chrono::seconds(60);
    auto& times = m_adoptions[client->getName()];
    times.push_back(now);
    while (!times.empty() && now - times.front() > window) {
        times.pop_front();
    }
    if (times.size() >= 4) {
        LOG_WARN("fork: client \"%s\" reconnected %zu times in the last minute"
                 " — connect/drop flapping; check the client-side fork log"
                 " (inputleap_fork_debug.log in the client's temp dir) for the"
                 " exit reason", client->getName().c_str(), times.size());
    }
}

void ServerExtension::fork_switchToScreen(const std::string& name) {
    Server* srv = host();
    Server::SwitchToScreenInfo info{name};
    srv->m_events->add_event(EventType::SERVER_SWITCH_TO_SCREEN, &srv->input_filter_,
                             create_event_data<Server::SwitchToScreenInfo>(info));
}

} // namespace inputleap
