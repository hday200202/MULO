#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "FileTree.hpp"
#include "MULOComponent.hpp"
// #include "Components/KBShortcuts.hpp"
// #include "Components/TimelineComponent.hpp"
// #include "Components/AppControls.hpp"
// #include "Components/FileBrowserComponent.hpp"
// #include "Components/SettingsComponent.hpp"
// #include "Components/MixerComponent.hpp"

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

// Platform-specific includes for dynamic loading
#ifdef _WIN32
    #include <windows.h>
    #define PLUGIN_EXT ".dll"
#elif __APPLE__
    #include <dlfcn.h>
    #define PLUGIN_EXT ".dylib"
#else
    #include <dlfcn.h>
    #define PLUGIN_EXT ".so"
#endif

// Forward declarations
class Application;
class Engine;
struct UIResources;
struct UIState;

class Application {
public:
    Column* baseContainer = nullptr;
    Row* mainContentRow = nullptr;
    bool shouldForceUpdate = false;
    bool freshRebuild = false;
    std::unique_ptr<UILO> ui = nullptr;

    Application();
    ~Application();

    void update();
    void render();
    void handleEvents();
    inline bool isRunning() const { return running; }

    inline Container* getComponentLayout(const std::string& componentName) { 
        if (muloComponents.find(componentName) != muloComponents.end()) 
            return muloComponents[componentName]->getLayout(); 
        return nullptr;
    }
    
    inline MULOComponent* getComponent(const std::string& componentName) {
        auto it = muloComponents.find(componentName);
        return (it != muloComponents.end()) ? it->second.get() : nullptr;
    }
    
    // Get base container for a specific page
    inline Container* getPageBaseContainer() { return baseContainer; }
    
    // Get main content row for component layout
    inline Row* getMainContentRow() { return mainContentRow; }
    
    // Helper to set parent container for components during initialization
    void setComponentParentContainer(const std::string& componentName, Container* parent);

    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters);

    inline const sf::RenderWindow& getWindow() const { return window; }
    inline void requestUIRebuild() { pendingUIRebuild = true; }
    inline void requestFullscreenToggle() { pendingFullscreenToggle = true; }

private:
    sf::Clock deltaClock;
    std::unordered_map<std::string, std::unique_ptr<Page>> uiloPages;
    std::unordered_map<std::string, std::unique_ptr<MULOComponent>> muloComponents;

    sf::RenderWindow window;
    sf::View windowView;
    sf::VideoMode screenResolution;

    // Core engine and UI
    Engine engine;
    UIState uiState;
    UIResources resources;

    bool running = false;
    bool fullscreen = false;
    bool pendingUIRebuild = false;
    bool pendingFullscreenToggle = false;

    size_t forceUpdatePoll = 0;

    // Plugin management
    struct LoadedPlugin {
        void* handle;           // Library handle
        PluginVTable* plugin;   // Plugin interface
        std::string path;       // Plugin file path
        std::string name;       // Plugin name (cached)
    };
    
    std::unordered_map<std::string, LoadedPlugin> loadedPlugins;

    void initUI();
    void initUIResources();
    void createWindow();
    void loadComponents();
    void rebuildUI();
    void toggleFullscreen();
    void cleanup();
    
    // Plugin loading methods
    void scanAndLoadPlugins();
    bool loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void unloadAllPlugins();
};