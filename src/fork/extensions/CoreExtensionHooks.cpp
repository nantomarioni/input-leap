/*
 * InputLeap Fork -- Core Extension Hooks Implementation
 * Copyright (C) 2024 InputLeap Fork Developers
 */

#include "CoreExtensionHooks.h"
#include "ScreenExtensionRegistry.h"
#include "ScreenDimmingExtension.h"
#include "base/Log.h"

namespace inputleap {

bool CoreExtensionHooks::s_initialized = false;

void CoreExtensionHooks::initialize() {
    if (s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: initializing fork extensions");
    
    // Register fork-specific extensions
    auto& registry = ScreenExtensionRegistry::getInstance();
    
    registry.registerExtension("screen_dimming", ScreenDimmingExtension::create);
    LOG_DEBUG("CoreExtensionHooks: registered screen dimming extension");
    
    // Initialize all registered extensions
    registry.initializeExtensions();
    
    s_initialized = true;
    LOG_DEBUG("CoreExtensionHooks: fork extensions initialized");
}

void CoreExtensionHooks::shutdown() {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: shutting down fork extensions");
    ScreenExtensionRegistry::getInstance().shutdownExtensions();
    s_initialized = false;
}

void CoreExtensionHooks::onScreenSwitched(const std::string& screenName, bool isActive) {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: screen switched - %s (active=%s)", 
              screenName.c_str(), isActive ? "true" : "false");
    
    auto& registry = ScreenExtensionRegistry::getInstance();
    if (isActive) {
        registry.notifyScreenActivated();
    } else {
        registry.notifyScreenDeactivated();
    }
}

void CoreExtensionHooks::onClientConnected(const std::string& clientName) {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: client connected - %s", clientName.c_str());
    // Future: notify extensions about client connections
}

void CoreExtensionHooks::onClientDisconnected(const std::string& clientName) {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: client disconnected - %s", clientName.c_str());
    // Future: notify extensions about client disconnections
}

void CoreExtensionHooks::onLocalInputDetected(const std::string& screenName) {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: local input detected on %s", screenName.c_str());
    ScreenExtensionRegistry::getInstance().notifyLocalInputDetected();
}

void CoreExtensionHooks::onConfigurationChanged() {
    if (!s_initialized) {
        return;
    }
    
    LOG_DEBUG("CoreExtensionHooks: configuration changed");
    // Future: notify extensions about configuration changes
}

} // namespace inputleap
