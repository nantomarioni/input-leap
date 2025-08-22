/*
 * InputLeap Fork -- Screen Dimming Extension
 * Copyright (C) 2024 InputLeap Fork Developers
 */

#include "ScreenDimmingExtension.h"
#include "base/Log.h"
#include <memory>

namespace inputleap {

ScreenDimmingExtension::ScreenDimmingExtension()
    : m_enabled(true)
    , m_dimmingPercentage(70)
    , m_isDimmed(false)
#ifdef _WIN32
    , m_gammaStored(false)
#endif
{
}

ScreenDimmingExtension::~ScreenDimmingExtension() {
    if (m_isDimmed) {
        applyDim(false);
    }
}

void ScreenDimmingExtension::onScreenActivated() {
    LOG_DEBUG("ScreenDimmingExtension: screen activated - restoring brightness");
    applyDim(false);
}

void ScreenDimmingExtension::onScreenDeactivated() {
    LOG_DEBUG("ScreenDimmingExtension: screen deactivated - dimming");
    applyDim(true);
}

bool ScreenDimmingExtension::onLocalInputDetected() {
    LOG_DEBUG("ScreenDimmingExtension: local input detected - restoring brightness");
    if (m_isDimmed) {
        applyDim(false);
        return true;
    }
    return false;
}

void ScreenDimmingExtension::initialize() {
    LOG_DEBUG("ScreenDimmingExtension: initializing");
    // Extension is ready to use
}

void ScreenDimmingExtension::shutdown() {
    LOG_DEBUG("ScreenDimmingExtension: shutting down");
    if (m_isDimmed) {
        applyDim(false);
    }
}

void ScreenDimmingExtension::setOption(const std::string& name, int value) {
    if (name == "enabled") {
        m_enabled = (value != 0);
        LOG_DEBUG("ScreenDimmingExtension: enabled = %s", m_enabled ? "true" : "false");
    } else if (name == "percentage") {
        m_dimmingPercentage = value;
        if (m_dimmingPercentage < 10) m_dimmingPercentage = 10;
        if (m_dimmingPercentage > 90) m_dimmingPercentage = 90;
        LOG_DEBUG("ScreenDimmingExtension: percentage = %d%%", m_dimmingPercentage);
    }
}

bool ScreenDimmingExtension::getOption(const std::string& name, int& value) const {
    if (name == "enabled") {
        value = m_enabled ? 1 : 0;
        return true;
    } else if (name == "percentage") {
        value = m_dimmingPercentage;
        return true;
    }
    return false;
}

ScreenExtensionPtr ScreenDimmingExtension::create() {
    return std::make_shared<ScreenDimmingExtension>();
}

// Generic command handler (used for dimming requests)
bool ScreenDimmingExtension::handleCommand(const std::string& cmd, const OptionsList& args) {
    if (cmd == "dim") {
        bool doDim = false;
        int percentage = -1;
        if (!args.empty()) {
            doDim = (args[0] != 0);
        }
        if (args.size() >= 2) {
            percentage = static_cast<int>(args[1]);
        }

        // Update percentage if provided
        if (percentage > 0) {
            m_dimmingPercentage = percentage;
            if (m_dimmingPercentage < 10) m_dimmingPercentage = 10;
            if (m_dimmingPercentage > 90) m_dimmingPercentage = 90;
            LOG_DEBUG("ScreenDimmingExtension: handleCommand set percentage = %d%%", m_dimmingPercentage);
        }

        applyDim(doDim);
        return true;
    }
    else if (cmd == "setOptions") {
        bool handled = false;
        // OptionsList is pairs of (OptionID, value)
        for (std::size_t i = 0; i + 1 < args.size(); i += 2) {
            std::uint32_t opt = args[i];
            std::uint32_t val = args[i + 1];
            if (opt == kOptionScreenDimmingEnabled) {
                m_enabled = (val != 0);
                LOG_DEBUG("ScreenDimmingExtension: option DMEN set to %d", m_enabled ? 1 : 0);
                handled = true;
            } else if (opt == kOptionScreenDimmingPercentage) {
                int percentage = static_cast<int>(val);
                if (percentage < 10) percentage = 10;
                if (percentage > 100) percentage = 100;
                m_dimmingPercentage = percentage;
                LOG_DEBUG("ScreenDimmingExtension: option DMPC set to %d", m_dimmingPercentage);
                handled = true;
            }
        }
        return handled;
    }
    return false;
}

// Private platform-specific implementation
void ScreenDimmingExtension::applyDim(bool dim) {
    if (!m_enabled) {
        return;
    }

    if (dim == m_isDimmed) {
        return; // Already in desired state
    }

#ifdef _WIN32
    HDC hdc = GetDC(nullptr);
    if (hdc != nullptr) {
        if (dim && !m_isDimmed) {
            // Store original gamma before dimming
            if (GetDeviceGammaRamp(hdc, m_originalGamma)) {
                float dimFactor = m_dimmingPercentage / 100.0f;
                LOG_DEBUG("ScreenDimmingExtension: dimming screen to %d%%", m_dimmingPercentage);
                
                // Create dimmed gamma ramp
                WORD dimmedGamma[256 * 3];
                for (int i = 0; i < 256 * 3; i++) {
                    dimmedGamma[i] = static_cast<WORD>(m_originalGamma[i] * dimFactor);
                }
                
                if (SetDeviceGammaRamp(hdc, dimmedGamma)) {
                    m_isDimmed = true;
                    m_gammaStored = true;
                    LOG_DEBUG("ScreenDimmingExtension: screen dimmed successfully");
                } else {
                    LOG_DEBUG("ScreenDimmingExtension: failed to set gamma ramp for dimming");
                }
            } else {
                LOG_DEBUG("ScreenDimmingExtension: failed to get current gamma ramp");
            }
        }
        else if (!dim && m_isDimmed && m_gammaStored) {
            LOG_DEBUG("ScreenDimmingExtension: restoring screen brightness");
            // Restore original gamma
            if (SetDeviceGammaRamp(hdc, m_originalGamma)) {
                m_isDimmed = false;
                LOG_DEBUG("ScreenDimmingExtension: screen brightness restored successfully");
            } else {
                LOG_DEBUG("ScreenDimmingExtension: failed to restore gamma ramp");
            }
        }
        ReleaseDC(nullptr, hdc);
    } else {
        LOG_DEBUG("ScreenDimmingExtension: failed to get device context");
    }
#else
    // Non-Windows platforms - placeholder for future implementation
    LOG_DEBUG("ScreenDimmingExtension: dimming not yet implemented for this platform");
#endif
}

} // namespace inputleap
