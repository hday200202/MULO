#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <fcntl.h>
#include <unistd.h>
#include <thread>

class PluginSandbox {
private:
    static std::unordered_set<std::string>& getSandboxedPlugins() {
        static std::unordered_set<std::string> sandboxedPlugins;
        return sandboxedPlugins;
    }
    
    static thread_local std::string currentThreadPlugin;
    
public:
    static bool isSandboxActive() { 
        return !currentThreadPlugin.empty() && 
               getSandboxedPlugins().find(currentThreadPlugin) != getSandboxedPlugins().end(); 
    }
    
    static std::string getCurrentPlugin() { return currentThreadPlugin; }
    
    static bool enableSandbox(const std::string& pluginName) {
        getSandboxedPlugins().insert(pluginName);
        currentThreadPlugin = pluginName;
        
        return true;
    }
    
    static bool disableSandbox() {
        std::string prevPlugin = currentThreadPlugin;
        if (!prevPlugin.empty()) {
            currentThreadPlugin = "";
            
        }
        return true;
    }
    
    static void setCurrentThreadPlugin(const std::string& pluginName) {
        currentThreadPlugin = pluginName;
    }
    
    static bool isPluginSandboxed(const std::string& pluginName) {
        auto& sandboxed = getSandboxedPlugins();
        bool result = sandboxed.find(pluginName) != sandboxed.end();
        
        return result;
    }
    
    static void removeSandboxedPlugin(const std::string& pluginName) {
        getSandboxedPlugins().erase(pluginName);
        if (currentThreadPlugin == pluginName) {
            currentThreadPlugin = "";
        }
    }
};