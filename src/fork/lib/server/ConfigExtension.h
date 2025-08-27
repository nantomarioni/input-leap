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

#include "inputleap/option_types.h"
#include <string>

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

protected:
    class Config* host() const;
};

} // namespace inputleap
