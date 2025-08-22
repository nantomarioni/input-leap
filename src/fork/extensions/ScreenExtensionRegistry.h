/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 */

#pragma once

#include "IScreenExtension.h"
#include <vector>
#include <map>
#include <string>

namespace inputleap {

/**
 * @brief Registry for screen extensions
 * 
 * Manages registration and lifecycle of screen extensions.
 * Allows decoupled addition of features without core modification.
 */
class ScreenExtensionRegistry {
public:
    static ScreenExtensionRegistry& getInstance();
    
    // Extension registration
    void registerExtension(const std::string& name, ScreenExtensionFactory factory);
    void unregisterExtension(const std::string& name);
    
    // Extension management
    void initializeExtensions();
    void shutdownExtensions();
    
    // Extension events
    void notifyScreenActivated();
    void notifyScreenDeactivated();
    void notifyLocalInputDetected();
    
    // Configuration
    void setExtensionOption(const std::string& extensionName, const std::string& optionName, int value);
    bool getExtensionOption(const std::string& extensionName, const std::string& optionName, int& value) const;
    
private:
    ScreenExtensionRegistry() = default;
    std::map<std::string, ScreenExtensionFactory> m_factories;
    std::map<std::string, ScreenExtensionPtr> m_extensions;
};

} // namespace inputleap
