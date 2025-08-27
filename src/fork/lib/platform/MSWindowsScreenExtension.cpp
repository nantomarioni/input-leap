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

#include "MSWindowsScreenExtension.h"
#include "platform/MSWindowsScreen.h"
#include "inputleap/option_types.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "base/LogExtension.h"

namespace inputleap {

MSWindowsScreenExtension::MSWindowsScreenExtension(): 
    m_isDimmed(false),
    m_dimmingEnabled(true),
    m_dimmingPercentage(70) {}
MSWindowsScreenExtension::~MSWindowsScreenExtension() {
    fork_dimScreen(false);
}

MSWindowsScreen* MSWindowsScreenExtension::host() const {
    return static_cast<MSWindowsScreen*>(const_cast<MSWindowsScreenExtension*>(this));
}

void MSWindowsScreenExtension::fork_dimScreen(bool dim) {
    // Debug: log every call to fork_dimScreen
    LOG_DEBUG("MSWindowsScreenExtension::fork_dimScreen called: dim=%d, enabled=%d, isDimmed=%d, percent=%d", dim ? 1 : 0, m_dimmingEnabled ? 1 : 0, m_isDimmed ? 1 : 0, m_dimmingPercentage);

    // Check if dimming is enabled
    if (!m_dimmingEnabled) return;

    // Obtain the MSWindowsScreen from the host pointer and operate on gamma ramp
    MSWindowsScreen* screen = host();
    if (!screen) return;

    // Call into the platform to get HDC if available; use ::GetDC as a fallback.
    HDC hdc = ::GetDC(nullptr);
    if (hdc != nullptr) {
        if (dim && !m_isDimmed) {
            // Store original gamma before dimming
            if (GetDeviceGammaRamp(hdc, reinterpret_cast<LPVOID>(m_originalGamma.data()))) {
                float dimFactor = m_dimmingPercentage / 100.0f;
                std::uint16_t dimmedGamma[256 * 3];
                for (int i = 0; i < 256 * 3; i++) {
                    dimmedGamma[i] = static_cast<std::uint16_t>(m_originalGamma[i] * dimFactor);
                }
                if (SetDeviceGammaRamp(hdc, reinterpret_cast<LPVOID>(dimmedGamma))) {
                    m_isDimmed = true;
                    LOG_DEBUG("MSWindowsScreenExtension: dimmed screen to %d%%", m_dimmingPercentage);
                }
            }
        }
        else if (!dim && m_isDimmed) {
            // Restore original gamma
            if (SetDeviceGammaRamp(hdc, reinterpret_cast<LPVOID>(m_originalGamma.data()))) {
                m_isDimmed = false;
                LOG_DEBUG("MSWindowsScreenExtension: restored original gamma");
            }
        }
        ReleaseDC(nullptr, hdc);
    }
}

void MSWindowsScreenExtension::fork_setOptions(const OptionsList& options) {
    for (std::uint32_t i = 0, n = static_cast<std::uint32_t>(options.size()); i < n; i += 2) {
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

bool MSWindowsScreenExtension::fork_onMouseMove(std::int32_t mx, std::int32_t my, bool warp) {
    return false;
}

} // namespace inputleap
