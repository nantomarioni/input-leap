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

#include "OSXScreenExtension.h"
#include "platform/OSXScreen.h"
#include <cstdio>

#include "base/LogExtension.h"

namespace inputleap {

OSXScreenExtension::OSXScreenExtension(): 
    m_isDimmed(false),
    m_dimmingEnabled(true),
    m_dimmingPercentage(70) {}
OSXScreenExtension::~OSXScreenExtension() {
	fork_dimScreen(false);
}

OSXScreen* OSXScreenExtension::host() const {
    return static_cast<OSXScreen*>(const_cast<OSXScreenExtension*>(this));
}

void OSXScreenExtension::fork_dimScreen(bool dim) {
	if (!m_dimmingEnabled) return;
	
	if (dim == m_isDimmed) return;
	
	// Get all active displays
	CGDisplayCount displayCount = 0;
	if (CGGetActiveDisplayList(0, nullptr, &displayCount) != CGDisplayNoErr || displayCount == 0) {
		FORK_LOG("failed to get display count");
		return;
	}

	CGDirectDisplayID* displays = new CGDirectDisplayID[displayCount];
	if (displays == nullptr) {
		FORK_LOG("failed to allocate display array");
		return;
	}

	if (CGGetActiveDisplayList(displayCount, displays, &displayCount) != CGDisplayNoErr) {
		FORK_LOG("failed to get display list");
		delete[] displays;
		return;
	}
	
	if (dim && !m_isDimmed) {
		FORK_LOG("attempting to dim %u displays", displayCount);
		
		// Clear any existing gamma info and prepare for new displays
		m_displayGammaInfo.clear();
		m_displayGammaInfo.reserve(displayCount);
		
		bool anySuccess = false;
		
		for (CGDisplayCount i = 0; i < displayCount; ++i) {
			CGDirectDisplayID display = displays[i];
			DisplayGammaInfo gammaInfo;
			gammaInfo.displayID = display;
			gammaInfo.gammaStored = false;
			
			// Store original gamma values for this display
			uint32_t sampleCount;
			CGError result = CGGetDisplayTransferByTable(display, 256, 
				gammaInfo.originalRed, gammaInfo.originalGreen, gammaInfo.originalBlue, &sampleCount);
			
			if (result == kCGErrorSuccess && sampleCount == 256) {
				gammaInfo.gammaStored = true;
				float dimFactor = m_dimmingPercentage / 100.0f;
				FORK_LOG("successfully got gamma tables for display %u, creating %d%% dimmed version", display, m_dimmingPercentage);
				
				// Create dimmed gamma tables using configurable percentage
				CGGammaValue dimRed[256], dimGreen[256], dimBlue[256];
				for (int j = 0; j < 256; j++) {
					dimRed[j] = gammaInfo.originalRed[j] * dimFactor;
					dimGreen[j] = gammaInfo.originalGreen[j] * dimFactor;
					dimBlue[j] = gammaInfo.originalBlue[j] * dimFactor;
				}
				
				// Apply dimmed gamma tables to this display
				result = CGSetDisplayTransferByTable(display, 256, dimRed, dimGreen, dimBlue);
				if (result == kCGErrorSuccess) {
					FORK_LOG("display %u dimmed to %d%% brightness successfully", display, m_dimmingPercentage);
					anySuccess = true;
				} else {
					FORK_LOG("failed to set gamma tables for display %u, error=%d", display, result);
					gammaInfo.gammaStored = false; // Reset since we failed
				}
			} else {
				FORK_LOG("failed to get current gamma tables for display %u, error=%d, sampleCount=%d", display, result, sampleCount);
			}
			
			// Store the gamma info regardless of success for proper cleanup
			m_displayGammaInfo.push_back(gammaInfo);
		}
		
		if (anySuccess) {
			m_isDimmed = true;
			FORK_LOG("successfully dimmed at least one display");
		} else {
			FORK_LOG("failed to dim any displays");
			m_displayGammaInfo.clear(); // Clear if nothing worked
		}
		
	} else if (!dim && m_isDimmed) {
		FORK_LOG("attempting to restore screen brightness for %zu displays", m_displayGammaInfo.size());
		
		bool anySuccess = false;
		
		// Restore original gamma tables for all displays
		for (auto& gammaInfo : m_displayGammaInfo) {
			if (gammaInfo.gammaStored) {
				CGError result = CGSetDisplayTransferByTable(gammaInfo.displayID, 256, 
					gammaInfo.originalRed, gammaInfo.originalGreen, gammaInfo.originalBlue);
				if (result == kCGErrorSuccess) {
					FORK_LOG("display %u brightness restored successfully", gammaInfo.displayID);
					anySuccess = true;
				} else {
					FORK_LOG("failed to restore gamma tables for display %u, error=%d", gammaInfo.displayID, result);
				}
			}
		}
		
		if (anySuccess) {
			m_isDimmed = false;
		}
		
		// Clear gamma info after restoration attempt
		m_displayGammaInfo.clear();
	}
	
	delete[] displays;
}

double OSXScreenExtension::fork_getLocalIdleSeconds() const {
	// Seconds since the last physical HID event (keyboard/mouse/trackpad).
	// kCGEventSourceStateHIDSystemState reflects hardware input; querying it
	// needs no additional TCC permissions.
	return CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState,
	                                              kCGAnyInputEventType);
}

void OSXScreenExtension::fork_setOptions(const OptionsList& options) {    for (std::uint32_t i = 0, n = static_cast<std::uint32_t>(options.size()); i < n; i += 2) {
		if (options[i] == kOptionScreenDimmingEnabled) {
			m_dimmingEnabled = (options[i + 1] != 0);
		}
		else if (options[i] == kOptionScreenDimmingPercentage) {
			m_dimmingPercentage = static_cast<int>(options[i + 1]);
			// Clamp to valid range
			if (m_dimmingPercentage < 10) m_dimmingPercentage = 10;
			if (m_dimmingPercentage > 100) m_dimmingPercentage = 100;
		}
	}
}

} // namespace inputleap
