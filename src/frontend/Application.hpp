#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "FileTree.hpp"
#include "MULOComponent.hpp"
#include <iostream>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <chrono>
#include <thread>
#include <list>

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

class Application;
class Engine;
struct UIResources;
struct UIState;

class Application : public juce::JUCEApplication {
public:
    Column* baseContainer = nullptr;
    Row* mainContentRow = nullptr;
    bool shouldForceUpdate = false;
    bool freshRebuild = false;
    bool openEffectWindow = false;
    std::unique_ptr<UILO> ui = nullptr;
    UIState uiState;
    UIResources resources;

    // JUCE Application interface
    const juce::String getApplicationName() override { return "MDAW"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override;
    void shutdown() override;

    Application();
    ~Application();

    // Core application methods
    void update();
    void render();
    void handleEvents();
    inline bool isRunning() const { return running; }

    // Component management
    inline Container* getComponentLayout(const std::string& componentName) { 
        if (muloComponents.find(componentName) != muloComponents.end()) 
            return muloComponents[componentName]->getLayout(); 
        return nullptr;
    }
    inline MULOComponent* getComponent(const std::string& componentName) {
        auto it = muloComponents.find(componentName);
        return (it != muloComponents.end()) ? it->second.get() : nullptr;
    }
    inline Container* getPageBaseContainer() { return baseContainer; }
    inline Row* getMainContentRow() { return mainContentRow; }
    void setComponentParentContainer(const std::string& componentName, Container* parent);

    // File operations
    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters);

    // Window management
    inline const sf::RenderWindow& getWindow() const { return window; }
    inline void requestUIRebuild() { pendingUIRebuild = true; }
    inline void requestFullscreenToggle() { pendingFullscreenToggle = true; }

    // Engine interface - Track management
    inline Track* getMasterTrack() { return engine.getMasterTrack(); }
    inline Track* getTrack(const std::string& name) { return engine.getTrackByName(name); }
    inline std::vector<std::unique_ptr<Track>>& getAllTracks() { return engine.getAllTracks(); }
    inline void addTrack(const std::string& name, const std::string& samplePath) { engine.addTrack(name, samplePath); }
    inline void removeTrack(const std::string& name) { engine.removeTrackByName(name); }

    // VST Effect management
    inline void addEffect(const std::string& filePath) {
        pendingEffectPath = filePath;
        hasPendingEffect = true;
        std::cout << "Queued effect for loading: " << filePath << std::endl;
    }
    inline void requestOpenEffectWindow(size_t effectIndex) {
        pendingEffectWindowIndex = effectIndex;
        hasPendingEffectWindow = true;
        std::cout << "Queued effect window opening for index: " << effectIndex << std::endl;
    }

    // Method to defer VST loading for save file restoration
    inline void deferEffectLoading(const std::string& trackName, const std::string& vstPath, bool openWindow = false, bool enabled = true, int index = -1, const std::vector<std::pair<int, float>>& parameters = {}) {
        DeferredEffect def;
        def.trackName = trackName;
        def.vstPath = vstPath;
        def.shouldOpenWindow = openWindow;
        def.enabled = enabled;
        def.index = index;
        def.parameters = parameters;
        
        deferredEffects.push_back(def);
        hasDeferredEffects = true;
        std::cout << "Deferred effect loading: " << vstPath << " for track: " << trackName << std::endl;
    }

    // Playback control
    inline void play() { engine.play(); }
    inline void pause() { engine.pause(); }
    inline void setSavedPosition(double seconds) { engine.setPosition(seconds); }
    inline bool isPlaying() const { return engine.isPlaying(); }
    inline void setBpm(float bpm) { engine.setBpm(bpm); }
    inline float getBpm() const { return engine.getBpm(); }
    inline double getPosition() const { return engine.getPosition(); }
    inline void setPosition(double seconds) { engine.setPosition(seconds); }

    // Audio clip management
    inline AudioClip* getReferenceClip(const std::string& trackName) { return engine.getTrackByName(trackName)->getReferenceClip(); }
    inline void addClipToTrack(const std::string& trackName, const AudioClip& clip) { engine.getTrackByName(trackName)->addClip(clip); }
    inline void removeClipFromTrack(const std::string& trackName, size_t index) { engine.getTrackByName(trackName)->removeClip(index); }

    // Audio configuration
    inline double getSampleRate() const { return engine.getSampleRate(); }
    inline void setSampleRate(const double newSampleRate) { 
        DEBUG_PRINT("Application: Setting sample rate to " << newSampleRate << "Hz");
        uiState.sampleRate = newSampleRate;
        uiState.saveConfig(); 
        engine.configureAudioDevice(newSampleRate);
        DEBUG_PRINT("Application: Sample rate configuration completed");
    }

    // Selected track management
    inline void setSelectedTrack(const std::string& trackName) { engine.setSelectedTrack(trackName); }
    inline std::string getSelectedTrack() const { return engine.getSelectedTrack(); }
    inline Track* getSelectedTrackPtr() { return engine.getSelectedTrackPtr(); }
    inline bool hasSelectedTrack() const { return engine.hasSelectedTrack(); }

    // Composition management
    inline void loadComposition(const std::string& path) { engine.loadComposition(path); }
    inline std::string getCurrentCompositionName() const { return engine.getCurrentCompositionName(); }
    inline void saveState() { engine.saveState(); }
    inline void saveToFile(const std::string& path) const { engine.save(path); }

private:
    sf::Clock deltaClock;
    std::unordered_map<std::string, std::unique_ptr<Page>> uiloPages;
    std::unordered_map<std::string, std::unique_ptr<MULOComponent>> muloComponents;

    sf::RenderWindow window;
    sf::View windowView;
    sf::VideoMode screenResolution;
    sf::Vector2u minWindowSize;

    Engine engine;

    bool running = false;
    bool fullscreen = false;
    bool pendingUIRebuild = false;
    bool pendingFullscreenToggle = false;
    bool prevCtrlShftR;

    std::string pendingEffectPath;
    size_t pendingEffectWindowIndex = SIZE_MAX;
    bool hasPendingEffect = false;
    bool hasPendingEffectWindow = false;

    // Deferred effect loading structure for save file restoration
    struct DeferredEffect {
        std::string trackName;
        std::string vstPath;
        bool shouldOpenWindow;
        bool enabled = true;
        int index = -1;
        std::vector<std::pair<int, float>> parameters;  // parameter index, value pairs
    };
    std::vector<DeferredEffect> deferredEffects;
    bool hasDeferredEffects = false;

    size_t forceUpdatePoll = 0;

    struct LoadedPlugin {
        void* handle;
        PluginVTable* plugin;
        std::string path;
        std::string name;
    };
    
    std::string exeDirectory = "";
    std::unordered_map<std::string, LoadedPlugin> loadedPlugins;
    std::unordered_map<std::string, ComponentLayoutData> componentLayouts;

    void initUI();
    void initUIResources();
    void createWindow();
    void loadComponents();
    void rebuildUI();
    void toggleFullscreen();
    void cleanup();
    
    void scanAndLoadPlugins();
    bool loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void unloadAllPlugins();
    
    void testVSTLoading();
};