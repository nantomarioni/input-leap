/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 */

#include "ScreenExtensionRegistry.h"
#include "base/Log.h"

namespace inputleap {

ScreenExtensionRegistry& ScreenExtensionRegistry::getInstance() {
    static ScreenExtensionRegistry instance;
    return instance;
}

void ScreenExtensionRegistry::registerExtension(const std::string& name, ScreenExtensionFactory factory) {
    LOG_DEBUG("registering screen extension: %s", name.c_str());
    m_factories[name] = factory;
}

void ScreenExtensionRegistry::unregisterExtension(const std::string& name) {
    LOG_DEBUG("unregistering screen extension: %s", name.c_str());
    
    // Shutdown and remove if active
    auto extIt = m_extensions.find(name);
    if (extIt != m_extensions.end()) {
        extIt->second->shutdown();
        m_extensions.erase(extIt);
    }
    
    // Remove factory
    m_factories.erase(name);
}

void ScreenExtensionRegistry::initializeExtensions() {
    LOG_DEBUG("initializing screen extensions");
    
    for (const auto& factory : m_factories) {
        try {
            auto extension = factory.second();
            if (extension) {
                extension->initialize();
                m_extensions[factory.first] = extension;
                LOG_DEBUG("initialized extension: %s", factory.first.c_str());
            }
        } catch (const std::exception& e) {
            LOG_WARN("failed to initialize extension %s: %s", factory.first.c_str(), e.what());
        }
    }
}

void ScreenExtensionRegistry::shutdownExtensions() {
    LOG_DEBUG("shutting down screen extensions");
    
    for (auto& extension : m_extensions) {
        try {
            extension.second->shutdown();
        } catch (const std::exception& e) {
            LOG_WARN("error shutting down extension %s: %s", extension.first.c_str(), e.what());
        }
    }
    m_extensions.clear();
}

void ScreenExtensionRegistry::notifyScreenActivated() {
    for (auto& extension : m_extensions) {
        try {
            extension.second->onScreenActivated();
        } catch (const std::exception& e) {
            LOG_WARN("error in extension %s onScreenActivated: %s", extension.first.c_str(), e.what());
        }
    }
}

void ScreenExtensionRegistry::notifyScreenDeactivated() {
    for (auto& extension : m_extensions) {
        try {
            extension.second->onScreenDeactivated();
        } catch (const std::exception& e) {
            LOG_WARN("error in extension %s onScreenDeactivated: %s", extension.first.c_str(), e.what());
        }
    }
}

void ScreenExtensionRegistry::notifyLocalInputDetected() {
    for (auto& extension : m_extensions) {
        try {
            extension.second->onLocalInputDetected();
        } catch (const std::exception& e) {
            LOG_WARN("error in extension %s onLocalInputDetected: %s", extension.first.c_str(), e.what());
        }
    }
}

bool ScreenExtensionRegistry::dispatchCommandToExtensions(const std::string& cmd, const OptionsList& args) {
    // iterate in registration order (m_extensions is a map; to preserve registration order we iterate factories)
    for (const auto& factoryPair : m_factories) {
        const std::string& name = factoryPair.first;
        auto it = m_extensions.find(name);
        if (it == m_extensions.end()) continue;
        try {
            bool handled = it->second->handleCommand(cmd, args);
            if (handled) {
                LOG_DEBUG("command '%s' handled by extension %s", cmd.c_str(), name.c_str());
                return true;
            }
        } catch (const std::exception& e) {
            LOG_WARN("error dispatching command '%s' to extension %s: %s", cmd.c_str(), name.c_str(), e.what());
        }
    }
    return false;
}

void ScreenExtensionRegistry::setExtensionOption(const std::string& extensionName, const std::string& optionName, int value) {
    auto it = m_extensions.find(extensionName);
    if (it != m_extensions.end()) {
        it->second->setOption(optionName, value);
    }
}

bool ScreenExtensionRegistry::getExtensionOption(const std::string& extensionName, const std::string& optionName, int& value) const {
    auto it = m_extensions.find(extensionName);
    if (it != m_extensions.end()) {
        return it->second->getOption(optionName, value);
    }
    return false;
}

ScreenExtensionPtr ScreenExtensionRegistry::getExtension(const std::string& name) const {
    auto it = m_extensions.find(name);
    if (it != m_extensions.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace inputleap
