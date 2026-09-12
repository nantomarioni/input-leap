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

class IPlatformScreen;

class PlatformScreenExtension {
public:
    PlatformScreenExtension();
    virtual ~PlatformScreenExtension();

    virtual void fork_dimScreen(bool dim) = 0;

    // Seconds since the last *physical* (HID) input on this machine, or a
    // negative value when the platform doesn't support the query. Used by
    // the client to detect local activity while dimmed (undim-on-touch).
    virtual double fork_getLocalIdleSeconds() const;
};

} // namespace inputleap
