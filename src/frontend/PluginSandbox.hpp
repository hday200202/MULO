#pragma once

#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>

class PluginSandbox {
private:
    static bool sandboxActive;
    static std::string currentPlugin;
public:
    static bool isSandboxActive() { return sandboxActive; }
    static std::string getCurrentPlugin() { return currentPlugin; }
    
    static bool enableSandbox(const std::string& pluginName) {
        sandboxActive = true;
        currentPlugin = pluginName;
        std::cout << "[SANDBOX] Enabled for plugin: " << pluginName << std::endl;
        return true;
    }
    
    static bool disableSandbox() {
        sandboxActive = false;
        std::string prevPlugin = currentPlugin;
        currentPlugin = "";
        if (!prevPlugin.empty()) {
            std::cout << "[SANDBOX] Disabled for plugin: " << prevPlugin << std::endl;
        }
        return true;
    }
};