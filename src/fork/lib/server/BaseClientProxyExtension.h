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

#include <string>

namespace inputleap {

class BaseClientProxy;

class BaseClientProxyExtension {
public:
    BaseClientProxyExtension();
    virtual ~BaseClientProxyExtension();

    virtual void fork_dimScreen(bool dim);
    virtual bool fork_parseMessage(const std::uint8_t* code);
    virtual bool fork_recvUndimRequest();

protected:
    class BaseClientProxy* host() const;
};

} // namespace inputleap
