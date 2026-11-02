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

#include <algorithm>
#include <ostream>
#include <sstream>

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

void ConfigExtension::fork_readSectionDisplays(ConfigReadContext& s) {
    Config* cfg = host();
    std::string line;
    while (s.readLine(line)) {
        if (line == "end") {
            return;
        }
        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos) {
            throw XConfigRead(s, "expected `name = x,y,w,h ...`");
        }
        std::string name = line.substr(0, eq);
        name.erase(name.find_last_not_of(" \t") + 1);
        const std::string canonical = cfg->getCanonicalName(name);
        if (canonical.empty()) {
            throw XConfigRead(s, "unknown screen name \"%{1}\"", name);
        }
        std::string numbers = line.substr(eq + 1);
        std::replace(numbers.begin(), numbers.end(), ',', ' ');
        std::istringstream in(numbers);
        std::vector<std::int32_t> values;
        std::int32_t value;
        while (in >> value) {
            values.push_back(value);
        }
        if (!in.eof() || values.empty() || values.size() % 4 != 0 || values.size() > 4 * 32) {
            throw XConfigRead(s, "displays of \"%{1}\" are not x,y,w,h groups", name);
        }
        std::vector<DisplayRect> displays;
        for (size_t i = 0; i < values.size(); i += 4) {
            DisplayRect d{values[i], values[i + 1], values[i + 2], values[i + 3]};
            if (d.w <= 0 || d.h <= 0) {
                throw XConfigRead(s, "a display of \"%{1}\" has no size", name);
            }
            displays.push_back(d);
        }
        m_displays[canonical] = displays;
    }
    throw XConfigRead(s, "unexpected end of displays section");
}

void ConfigExtension::fork_writeDisplays(std::ostream& s) const {
    if (m_displays.empty()) {
        return;
    }
    s << "section: displays\n";
    for (const auto& screen : m_displays) {
        s << "\t" << screen.first << " =";
        for (const DisplayRect& d : screen.second) {
            s << " " << d.x << "," << d.y << "," << d.w << "," << d.h;
        }
        s << "\n";
    }
    s << "end\n";
}

const std::vector<DisplayRect>* ConfigExtension::fork_displays(const std::string& name) const {
    const auto it = m_displays.find(host()->getCanonicalName(name));
    return it == m_displays.end() ? nullptr : &it->second;
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
