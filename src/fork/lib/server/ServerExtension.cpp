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
#include "server/Config.h"
#include "inputleap/option_types.h"
#include "base/Log.h"
#include "base/Event.h"
#include "base/IEventQueue.h"

#include <algorithm>

namespace inputleap {

ServerExtension::ServerExtension() {}
ServerExtension::~ServerExtension() {}

Server* ServerExtension::host() const {
    return static_cast<Server*>(const_cast<ServerExtension*>(this));
}

void ServerExtension::fork_dimScreenAll() {
    Server* srv = host();
    // dimming turned off in the configuration: nothing is dimmed (a client
    // told to dim would skip it but still say it dimmed)
    bool enabled = true;
    if (const Config::ScreenOptions* options = srv->m_config->getOptions("")) {
        const auto option = options->find(kOptionScreenDimmingEnabled);
        enabled = option == options->end() || option->second != 0;
    }
    for (const auto& clientPair : srv->m_clients) {
        BaseClientProxy* clientProxy = clientPair.second;
        if (clientProxy == nullptr) continue;
        clientProxy->fork_dimScreen(enabled && clientProxy != srv->m_active);
    }

    // The active screen may change while the cursor is locked (undim-on-
    // touch jumps bypass the lock), which changes who is unreachable —
    // re-evaluate. Per-proxy edge detection keeps this from spamming.
    if (srv->m_lockedToScreen) {
        fork_lockStateChanged(true);
    }
}

void ServerExtension::fork_clientAdopted(BaseClientProxy* client) {
    Server* srv = host();

    // Initial screen dimming: new clients should be dimmed if they are not
    // the active screen.
    if (client != srv->m_active) {
        fork_dimScreenAll();

        // Late joiner while the cursor is locked elsewhere: tell it.
        if (srv->m_lockedToScreen) {
            client->fork_sendLockState(true);
        }
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

void ServerExtension::fork_lockStateChanged(bool locked) {
    Server* srv = host();
    for (const auto& clientPair : srv->m_clients) {
        BaseClientProxy* clientProxy = clientPair.second;
        if (clientProxy == nullptr) continue;
        // A screen is "locked out" when the cursor is locked and it is not
        // the active screen. Sending the resulting per-screen state (rather
        // than skipping anyone) also clears stale warnings when the active
        // screen changes while locked; per-proxy dedup suppresses no-ops.
        clientProxy->fork_sendLockState(locked && clientProxy != srv->m_active);
    }
}

std::vector<DisplayRect> ServerExtension::fork_displaysOf(BaseClientProxy* client, DisplayRect& box) const {
    Server* srv = host();
    if (client == nullptr || srv->m_config == nullptr) {
        return {};
    }
    client->getShape(box.x, box.y, box.w, box.h);
    const std::string name = srv->getName(client);
    const std::vector<DisplayRect>* given = srv->m_config->fork_displays(name);
    if (given == nullptr || given->size() < 2) {
        return {};
    }
    std::int32_t right = 0, bottom = 0, left = given->front().x, top = given->front().y;
    for (const DisplayRect& d : *given) {
        left = (std::min)(left, d.x);
        top = (std::min)(top, d.y);
        right = (std::max)(right, d.x + d.w);
        bottom = (std::max)(bottom, d.y + d.h);
    }
    if (left != 0 || top != 0 || right != box.w || bottom != box.h) {
        if (m_displaysMismatch.insert(name).second) {
            LOG_WARN("the displays configured for \"%s\" span %dx%d, it reports %dx%d: using its whole desktop",
                     name.c_str(), right - left, bottom - top, box.w, box.h);
        }
        return {};
    }
    m_displaysMismatch.erase(name);
    std::vector<DisplayRect> displays = *given;
    for (DisplayRect& d : displays) {
        d.x += box.x;
        d.y += box.y;
    }
    return displays;
}

void ServerExtension::fork_enterDisplays(BaseClientProxy* dst, EDirection dir, std::int32_t& x, std::int32_t& y) const {
    DisplayRect box;
    const std::vector<DisplayRect> displays = fork_displaysOf(dst, box);
    display_geometry::enter(displays, box, dir, x, y);
}

void ServerExtension::fork_leaveDisplays(BaseClientProxy* client, std::int32_t xOld, std::int32_t yOld,
                                         std::int32_t& x, std::int32_t& y) const {
    DisplayRect box;
    const std::vector<DisplayRect> displays = fork_displaysOf(client, box);
    if (display_geometry::leave(displays, box, xOld, yOld, x, y)) {
        LOG_DEBUG2("off a display of \"%s\" facing out, at %d,%d", host()->getName(client).c_str(), x, y);
    }
}

void ServerExtension::fork_stayOnDisplays(BaseClientProxy* client, std::int32_t xOld, std::int32_t yOld,
                                          std::int32_t& x, std::int32_t& y) const {
    DisplayRect box;
    const std::vector<DisplayRect> displays = fork_displaysOf(client, box);
    display_geometry::stay(displays, xOld, yOld, x, y);
}

void ServerExtension::fork_toBoxEdge(BaseClientProxy* client, std::int32_t zone, std::int32_t& x, std::int32_t& y) const {
    DisplayRect box;
    const std::vector<DisplayRect> displays = fork_displaysOf(client, box);
    display_geometry::toBoxEdge(displays, box, zone, x, y);
}

} // namespace inputleap
