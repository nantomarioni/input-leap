/*
 * InputLeap Fork Extensions Integration
 * Minimal core integration points for custom features
 */

#pragma once

namespace inputleap {

/**
 * @brief Extension hooks for core functionality
 * 
 * These are the minimal integration points needed in core files.
 * Instead of spreading changes across many files, we concentrate them here.
 */
class CoreExtensionHooks {
public:
    // Screen event hooks
    static void onScreenSwitched(const std::string& screenName, bool isActive);
    static void onClientConnected(const std::string& clientName);
    static void onClientDisconnected(const std::string& clientName);
    static void onLocalInputDetected(const std::string& screenName);
    
    // Configuration hooks
    static void onConfigurationChanged();
    
    // Initialize/shutdown
    static void initialize();
    static void shutdown();
    
private:
    static bool s_initialized;
};

} // namespace inputleap
