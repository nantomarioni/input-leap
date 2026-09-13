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

#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <string>

namespace inputleap {

class Server;
class BaseClientProxy;

class ServerExtension {
public:
    ServerExtension();
    virtual ~ServerExtension();

    virtual void fork_dimScreenAll();

    // Called from Server::adoptClient for every accepted client. Applies the
    // initial dim state and warns when a client reconnects repeatedly
    // (flap detection for the connect/dim/drop loop).
    virtual void fork_clientAdopted(BaseClientProxy* client);

    // Post a switch-to-screen request the way Server expects it (the
    // handler is registered on the input filter's event target, which is
    // private to Server — hence this friend-powered helper).
    virtual void fork_switchToScreen(const std::string& name);

    // Broadcast the cursor-lock state. On lock, every screen EXCEPT the
    // active (locked) one gets notified — those are the screens that just
    // became unreachable. On unlock, everyone gets notified (the active
    // screen may have changed while locked via an explicit jump, so clear
    // unconditionally).
    virtual void fork_lockStateChanged(bool locked);

protected:
    class Server* host() const;

private:
    // client name -> recent adoption timestamps (flap detector)
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>> m_adoptions;
};

} // namespace inputleap
