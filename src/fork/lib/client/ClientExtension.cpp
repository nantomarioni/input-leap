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
#include "inputleap/ProtocolUtil.h"
#include "inputleap/protocol_types.h"
#include "base/IEventQueue.h"
#include "base/EventQueueTimer.h"
#include "base/EventTypes.h"
#include "base/Log.h"

namespace inputleap {

namespace {
// Poll cadence while dimmed; anything under this is "input just happened".
const double kPollIntervalSeconds = 0.5;
// Ignore input during the first moment after dimming: the dim itself (and
// any in-flight injected events from the switch away) must not re-trigger.
const std::chrono::milliseconds kDimDebounce{1200};
// Minimum spacing between undim requests (server may be slow to react).
const std::chrono::seconds kRequestCooldown{3};
} // namespace

ClientExtension::ClientExtension() :
    m_isDimmed(false),
    m_pollerEvents(nullptr),
    m_pollerTimer(nullptr)
{
}

ClientExtension::~ClientExtension() {
    // Client subobject is already destroyed here — must not call host().
    // stopUndimPoller() only uses the stored events pointer.
    stopUndimPoller();
}

Client* ClientExtension::host() const {
    return static_cast<Client*>(const_cast<ClientExtension*>(this));
}

void ClientExtension::fork_dimScreen(bool dim) {
    Client* c = host();
    if (!c) return;
    if (c->m_screen) c->m_screen->fork_dimScreen(dim);

    if (dim && !m_isDimmed) {
        m_dimmedAt = std::chrono::steady_clock::now();
        startUndimPoller();
    }
    else if (!dim && m_isDimmed) {
        stopUndimPoller();
    }
    m_isDimmed = dim;
}

void ClientExtension::startUndimPoller() {
    if (m_pollerTimer != nullptr) return;

    Client* c = host();
    if (!c || c->m_events == nullptr || c->m_screen == nullptr) return;

    // Only poll when the platform can answer the idle query at all.
    if (c->m_screen->fork_getLocalIdleSeconds() < 0.0) {
        LOG_DEBUG("fork: undim-on-touch unsupported on this platform, poller not started");
        return;
    }

    m_pollerEvents = c->m_events;
    m_pollerTimer = m_pollerEvents->newTimer(kPollIntervalSeconds, nullptr);
    m_pollerEvents->add_handler(EventType::TIMER, m_pollerTimer,
                                [this](const auto&){ pollLocalInput(); });
    LOG_DEBUG("fork: undim-on-touch poller started");
}

void ClientExtension::stopUndimPoller() {
    if (m_pollerTimer == nullptr) return;
    m_pollerEvents->remove_handler(EventType::TIMER, m_pollerTimer);
    m_pollerEvents->deleteTimer(m_pollerTimer);
    m_pollerTimer = nullptr;
    m_pollerEvents = nullptr;
    LOG_DEBUG("fork: undim-on-touch poller stopped");
}

void ClientExtension::pollLocalInput() {
    Client* c = host();
    if (!c || c->m_screen == nullptr || !m_isDimmed) return;

    const auto now = std::chrono::steady_clock::now();
    if (now - m_dimmedAt < kDimDebounce) {
        return; // settle period right after dimming
    }

    const double idle = c->m_screen->fork_getLocalIdleSeconds();
    if (idle < 0.0 || idle > kPollIntervalSeconds) {
        return; // no fresh physical input
    }

    if (now - m_lastUndimRequest < kRequestCooldown) {
        return;
    }
    m_lastUndimRequest = now;
    sendUndimRequest();
}

void ClientExtension::sendUndimRequest() {
    Client* c = host();
    if (!c || c->m_stream == nullptr || !c->m_ready) return;

    // NOTE level so it shows in the app's log window, not only the fork file.
    LOG_NOTE("fork: local input detected while dimmed - requesting undim");
    ProtocolUtil::writef(c->m_stream, kMsgDUndimRequest);
}

} // namespace inputleap
