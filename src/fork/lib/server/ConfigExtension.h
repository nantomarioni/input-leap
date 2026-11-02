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

#include "DisplayGeometry.h"
#include "inputleap/option_types.h"
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace inputleap {

class Config;
class ConfigReadContext;

class ConfigExtension {
public:
    ConfigExtension();
    virtual ~ConfigExtension();

    virtual bool fork_readSectionOptions(ConfigReadContext& s, std::string name, std::string value);
    static const char* fork_getOptionName(OptionID id);
    static std::string fork_getOptionValue(OptionID id, OptionValue value);

    // `section: displays`, one line per computer whose monitors don't fill
    // its desktop: `name = x,y,w,h x,y,w,h ...` in its own pixels, the box
    // around them starting at 0,0. Read after the screens.
    void fork_readSectionDisplays(ConfigReadContext& s);
    void fork_writeDisplays(std::ostream& s) const;
    // The monitors of a screen (canonical name), or null when not given.
    const std::vector<DisplayRect>* fork_displays(const std::string& name) const;
    bool fork_sameDisplays(const ConfigExtension& other) const { return m_displays == other.m_displays; }

protected:
    class Config* host() const;

private:
    std::map<std::string, std::vector<DisplayRect>> m_displays;
};

} // namespace inputleap
