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

#include "ConfigExtension.h"
#include "server/Config.h"

namespace inputleap {

ConfigExtension::ConfigExtension() {}
ConfigExtension::~ConfigExtension() {}

Config* ConfigExtension::host() const {
    return static_cast<Config*>(const_cast<ConfigExtension*>(this));
}

bool ConfigExtension::fork_readSectionOptions(ConfigReadContext& s, std::string name, std::string value) {
    Config* cfg = host();
    if (name == "screenDimmingEnabled") {
        cfg->addOption("", kOptionScreenDimmingEnabled, s.parseBoolean(value));
        return true;
    }
    else if (name == "screenDimmingPercentage") {
        cfg->addOption("", kOptionScreenDimmingPercentage, s.parseInt(value));
        return true;
    }
    return false;
}

const char* ConfigExtension::fork_getOptionName(OptionID id) {
    if (id == kOptionClipboardSharing) {
        return "clipboardSharing";
    }
    if (id == kOptionClipboardSharingSize) {
        return "clipboardSharingSize";
    }
    return nullptr;
}

std::string ConfigExtension::fork_getOptionValue(OptionID id, OptionValue value) {
    if (id == kOptionScreenDimmingEnabled) {
        return (value != 0) ? "true" : "false";
    }
    if (id == kOptionScreenDimmingPercentage) {
        return inputleap::string::sprintf("%d", value);
    }
    return "";
}

} // namespace inputleap
