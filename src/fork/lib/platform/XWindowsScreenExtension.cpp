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

#include "XWindowsScreenExtension.h"
#include "platform/XWindowsScreen.h"
#include "inputleap/option_types.h"

#include "base/LogExtension.h"

namespace inputleap {

XWindowsScreenExtension::XWindowsScreenExtension(): 
    m_dimmingEnabled(true),
    m_dimmingPercentage(70) {}
XWindowsScreenExtension::~XWindowsScreenExtension() {
    fork_dimScreen(false);
}

XWindowsScreen* XWindowsScreenExtension::host() const {
    return static_cast<XWindowsScreen*>(const_cast<XWindowsScreenExtension*>(this));
}

void XWindowsScreenExtension::fork_dimScreen(bool dim) {
    LOG_DEBUG("XWindowsScreen::dimScreen called with dim=%d", dim ? 1 : 0);
	
	// Check if dimming is enabled
	if (!m_dimmingEnabled) {
		LOG_DEBUG("screen dimming is disabled, skipping");
		return;
	}
	
	// TODO: Implement actual screen dimming for X11
	// For now, just log the intended operation with the percentage
	LOG_DEBUG("X11 screen dimming requested: %s to %d%% (not yet implemented)", 
			  dim ? "dim" : "restore", m_dimmingPercentage);
	
	// Potential implementations could use:
	// 1. XF86VidMode extension for gamma ramp manipulation
	// 2. XRandR for backlight control on newer systems
	// 3. DPMS for power management-based dimming
	// For now, this serves as a placeholder that respects the configuration
}

void XWindowsScreenExtension::fork_setOptions(const OptionsList& options) {
    for (std::uint32_t i = 0, n = options.size(); i < n; i += 2) {
		if (options[i] == kOptionScreenDimmingEnabled) {
			m_dimmingEnabled = (options[i + 1] != 0);
		}
		else if (options[i] == kOptionScreenDimmingPercentage) {
			m_dimmingPercentage = static_cast<int>(options[i + 1]);
			if (m_dimmingPercentage < 10) m_dimmingPercentage = 10;
			if (m_dimmingPercentage > 100) m_dimmingPercentage = 100;
		}
	}
}

} // namespace inputleap
