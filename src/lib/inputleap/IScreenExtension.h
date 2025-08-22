/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
// OptionsList typedef
#include "inputleap/option_types.h"

namespace inputleap {

/**
 * @brief Extension interface for screen functionality
 * 
 * This interface allows extending screen behavior without modifying core classes.
 * Custom features like screen dimming can be implemented as extensions.
 */
class IScreenExtension {
public:
    virtual ~IScreenExtension() = default;
    
    // Screen state callbacks
    virtual void onScreenActivated() {}
    virtual void onScreenDeactivated() {}
    // Called when local input is detected; return true if the extension handled the event
    virtual bool onLocalInputDetected() { return false; }
    
    // Extension lifecycle
    virtual void initialize() {}
    virtual void shutdown() {}
    
    // Configuration
    virtual void setOption(const std::string& name, int value) {}
    virtual bool getOption(const std::string& name, int& value) const { return false; }
    // Generic command interface for future extension actions (use this for dim/restore)
    // Return true if the extension handled the command.
    virtual bool handleCommand(const std::string& /*cmd*/, const OptionsList& /*args*/) { return false; }
};

using ScreenExtensionPtr = std::shared_ptr<IScreenExtension>;
using ScreenExtensionFactory = std::function<ScreenExtensionPtr()>;

} // namespace inputleap
