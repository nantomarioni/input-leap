/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 */

#pragma once

#include "IScreenExtension.h"
#include "base/Log.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace inputleap {

/**
 * @brief Screen dimming extension
 * 
 * Implements automatic screen dimming for inactive screens.
 * This is isolated from core functionality to minimize upstream conflicts.
 */
class ScreenDimmingExtension : public IScreenExtension {
public:
    ScreenDimmingExtension();
    ~ScreenDimmingExtension() override;
    
    // IScreenExtension interface
    void onScreenActivated() override;
    void onScreenDeactivated() override;
    void onLocalInputDetected() override;
    void initialize() override;
    void shutdown() override;
    void setOption(const std::string& name, int value) override;
    bool getOption(const std::string& name, int& value) const override;
    
    // Static factory function for registration
    static ScreenExtensionPtr create();
    
private:
    void dimScreen(bool dim);
    
    bool m_enabled;
    int m_dimmingPercentage;
    bool m_isDimmed;
    
#ifdef _WIN32
    WORD m_originalGamma[256 * 3];
    bool m_gammaStored;
#endif
};

} // namespace inputleap
