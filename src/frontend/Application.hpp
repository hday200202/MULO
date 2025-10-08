#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "FileTree.hpp"
#include "MULOComponent.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <juce_core/juce_core.h>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <juce_gui_basics/juce_gui_basics.h>
#include <chrono>
#include <thread>
#include <list>
#include "EmailService.hpp"

#ifdef FIREBASE_AVAILABLE
    #include <firebase/database.h>
#endif

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

#ifdef FIREBASE_AVAILABLE
#include <firebase/app.h>
#include <firebase/firestore.h>
#include <firebase/database.h>
#include <firebase/auth.h>
#endif

class Application;
class Engine;
struct UIResources;
struct UIState;
class MIDIClip;

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
    nlohmann::json config;


    const juce::String getApplicationName() override { return "MULO"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override;
    void shutdown() override;

    Application();
    ~Application();

    void update();
    void render();
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
    inline Container* getPageBaseContainer() { return baseContainer; }
    inline Row* getMainContentRow() { return mainContentRow; }
    void setComponentParentContainer(const std::string& componentName, Container* parent);

    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters);

    inline const sf::RenderWindow& getWindow() const { return window; }
    inline void requestUIRebuild() { pendingUIRebuild = true; }
    inline void requestFullscreenToggle() { pendingFullscreenToggle = true; }


    inline Track* getMasterTrack() { return engine.getMasterTrack(); }
    inline Track* getTrack(const std::string& name) { return engine.getTrackByName(name); }
    inline std::vector<std::unique_ptr<Track>>& getAllTracks() { return engine.getAllTracks(); }
    inline void addTrack(const std::string& name, const std::string& samplePath) { engine.addTrack(name, samplePath); }
    inline void removeTrack(const std::string& name) { pendingTrackRemoveName = name; }
    inline void exportAudio() {
        std::string path = selectDirectory();
        engine.exportMaster(path);
    }
    inline void setMetronomeEnabled(bool enabled) { engine.setMetronomeEnabled(enabled); }
    inline bool isMetronomeEnabled() const { return engine.isMetronomeEnabled(); }

    inline void playSound(const std::string& filePath, float db) { engine.playSound(filePath, db); }
    inline void playSound(const juce::File& file, float db) { engine.playSound(file, db); }

    inline std::string getEngineStateString() const { return engine.getStateString(); }
    inline void loadEngineStateString(const std::string& stateString) { engine.load(stateString); }
    inline std::string getEngineStateHash() const { return engine.getStateHash(); }

    inline void sendMIDINote(int noteNumber, int velocity, bool noteOn = true) {
        engine.sendRealtimeMIDI(noteNumber, velocity, noteOn);
    }

    inline void addEffect(const std::string& filePath) {
        pendingEffectPath = filePath;
        hasPendingEffect = true;
    }
    
    inline void addSynthesizer(const std::string& filePath) {
        pendingSynthPath = filePath;
        hasPendingSynth = true;
    }
    
    inline void requestOpenEffectWindow(size_t effectIndex) {
        pendingEffectWindowIndex = effectIndex;
        hasPendingEffectWindow = true;
    }

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
    }

    inline void play() { engine.play(); }
    inline void pause() { engine.pause(); }
    inline void setSavedPosition(double seconds) { engine.setPosition(seconds); }
    inline bool isPlaying() const { return engine.isPlaying(); }
    inline void setBpm(float bpm) { engine.setBpm(bpm); }
    inline float getBpm() const { return engine.getBpm(); }
    inline double getPosition() const { return engine.getPosition(); }
    inline double getSavedPosition() const { return engine.getSavedPosition(); }
    inline void setPosition(double seconds) { engine.setPosition(seconds); }
    inline std::pair<int, int> getTimeSignature() {return engine.getTimeSignature(); }

    inline AudioClip* getReferenceClip(const std::string& trackName) { return engine.getTrackByName(trackName)->getReferenceClip(); }
    inline void addClipToTrack(const std::string& trackName, const AudioClip& clip) { 
        engine.getTrackByName(trackName)->addClip(clip); 
        // Force state update after clip modification
        std::string currentRoom = readConfig<std::string>("collab_room", "");
        if (!currentRoom.empty()) {
            updateRoomEngineState(currentRoom, engine.getStateString());
        }
    }
    inline void removeClipFromTrack(const std::string& trackName, size_t index) { 
        engine.getTrackByName(trackName)->removeClip(index); 
        // Force state update after clip modification
        std::string currentRoom = readConfig<std::string>("collab_room", "");
        if (!currentRoom.empty()) {
            updateRoomEngineState(currentRoom, engine.getStateString());
        }
    }
    
    // Method for updating clip positions (for moves)
    inline void updateClipInTrack(const std::string& trackName, size_t index, const AudioClip& newClip) {
        auto* track = engine.getTrackByName(trackName);
        if (track && index < track->getClips().size()) {
            track->removeClip(index);
            track->addClip(newClip);
            // Force state update after clip modification
            std::string currentRoom = readConfig<std::string>("collab_room", "");
            if (!currentRoom.empty()) {
                updateRoomEngineState(currentRoom, engine.getStateString());
            }
        }
    }

    inline double getSampleRate() const { return engine.getSampleRate(); }
    inline void setSampleRate(const double newSampleRate) { 
        uiState.sampleRate = newSampleRate;
        writeConfig("sampleRate", newSampleRate);
        engine.configureAudioDevice(newSampleRate);
    }

    template<typename T>
    void writeConfig(const std::string& key, const T& value) {
        config[key] = value;
        saveConfig();
    }

    template<typename T>
    T readConfig(const std::string& key, const T& defaultValue = T{}) const {
        if (config.contains(key)) {
            try {
                return config[key].get<T>();
            } catch (const std::exception& e) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    void saveConfig();
    void loadConfig();
    
    void syncUIStateToConfig() {
        writeConfig("fileBrowserDirectory", uiState.fileBrowserDirectory);
        writeConfig("vstDirectory", uiState.vstDirecory);
        writeConfig("vstDirectories", uiState.vstDirectories);
        writeConfig("saveDirectory", uiState.saveDirectory);
        writeConfig("selectedTheme", uiState.selectedTheme);
        writeConfig("sampleRate", uiState.sampleRate);
        writeConfig("autoSaveIntervalSeconds", uiState.autoSaveIntervalSeconds);
        writeConfig("enableAutoVSTScan", uiState.enableAutoVSTScan);
    }
    void saveLayoutConfig();

    inline void setSelectedTrack(const std::string& trackName) { engine.setSelectedTrack(trackName); }
    inline std::string getSelectedTrack() const { return engine.getSelectedTrack(); }
    inline Track* getSelectedTrackPtr() { return engine.getSelectedTrackPtr(); }
    inline bool hasSelectedTrack() const { return engine.hasSelectedTrack(); }

    inline void loadComposition(const std::string& path) { engine.loadComposition(path); engine.generateMetronomeTrack(); }
    inline std::string getCurrentCompositionName() const { return engine.getCurrentCompositionName(); }
    inline void setCurrentCompositionName(const std::string& name) { engine.setCurrentCompositionName(name); }
    inline void saveState() { engine.save(); }
    inline void saveToFile(const std::string& path) const { engine.save(path); }

    MIDIClip* getSelectedMIDIClip() const;
    MIDIClip* getTimelineSelectedMIDIClip() const;

    // Parameter tracking for automation
    void updateParameterTracking();

    // Firebase methods for MarketplaceComponent
    struct ExtensionData {
        std::string id = "";
        std::string author = "Unknown";
        std::string description = "No description provided.";
        std::string downloadURL = "";
        std::string name = "Unnamed Extension";
        std::string version = "0.1.0";
        bool verified = false;
    };

    enum class FirebaseState { Idle, Loading, Success, Error };
    
    void initFirebase();
    void fetchExtensions(std::function<void(FirebaseState, const std::vector<ExtensionData>&)> callback);
    FirebaseState getFirebaseState() const { return firebaseState; }
    const std::vector<ExtensionData>& getExtensions() const { return extensions; }

    // User Authentication methods
    enum class AuthState { Idle, Loading, Success, Error, RequiresMFA };
    void registerUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback);
    void loginUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback);
    void verifyMFA(const std::string& verificationCode, std::function<void(AuthState, const std::string&)> callback);
    void enableMFA(std::function<void(AuthState, const std::string&)> callback);
    void logoutUser();
    bool isUserLoggedIn() const;
    std::string getCurrentUserEmail() const;
    
    // Session persistence
    void saveLastLoggedInUser(const std::string& email);
    std::string getLastLoggedInUser();
    bool isReturningUser(const std::string& email);
    
    // Collaboration methods
    void createRoom(const std::string& roomName);
    void readFromRoom(const std::string& roomName);
    void joinRoom(const std::string& roomName);
    void leaveRoom(const std::string& roomName);
    void updateRoomEngineState(const std::string& roomName, const std::string& engineState);
    void checkRoomEngineState(const std::string& roomName);
    void writeToRoom(const std::string& roomName, const std::string& section, const std::string& data);
    
    mutable std::mutex firebaseMutex;

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
    bool prevCtrlShftR = false;
    bool prevDragging = false;

    int draggedComponentIndex = -1;
    sf::RectangleShape dragOverlay;

    std::string pendingEffectPath;
    size_t pendingEffectWindowIndex = SIZE_MAX;
    bool hasPendingEffect = false;
    bool hasPendingEffectWindow = false;

    std::string pendingSynthPath;
    bool hasPendingSynth = false;

    std::string pendingTrackRemoveName = "";

    struct DeferredEffect {
        std::string trackName;
        std::string vstPath;
        bool shouldOpenWindow;
        bool enabled = true;
        int index = -1;
        std::vector<std::pair<int, float>> parameters;
    };
    std::vector<DeferredEffect> deferredEffects;
    bool hasDeferredEffects = false;

    size_t forceUpdatePoll = 0;

    struct LoadedPlugin {
        void* handle;
        PluginVTable* plugin;
        std::string path;
        std::string name;
        bool isSandboxed = true;
        bool isTrusted = false;
    };
    
    struct PluginSandboxConfig {
        bool enableSandboxing = true;
        bool allowFilesystemByDefault = false;
        bool allowNetworkByDefault = false;
        std::vector<std::string> trustedPlugins;
        std::vector<std::string> allowedPaths;
    } pluginSandboxConfig;
    
    std::string exeDirectory = "";
    std::unordered_map<std::string, LoadedPlugin> loadedPlugins;
    std::unordered_map<std::string, ComponentLayoutData> componentLayouts;

    // Firebase members
#ifdef FIREBASE_AVAILABLE
    std::unique_ptr<firebase::App> firebaseApp;
    firebase::firestore::Firestore* firestore = nullptr;
    firebase::database::Database* realtimeDatabase = nullptr;
    firebase::auth::Auth* auth = nullptr;
    firebase::Future<firebase::firestore::QuerySnapshot> extFuture;
#endif
    FirebaseState firebaseState = FirebaseState::Idle;
    std::vector<ExtensionData> extensions;
    std::function<void(FirebaseState, const std::vector<ExtensionData>&)> firebaseCallback;
    std::string lastKnownRemoteEngineState = "";
    
    // User Authentication members
    AuthState authState = AuthState::Idle;
    std::string currentUserEmail = "";
    bool userLoggedIn = false;
    bool mfaRequired = false;
    std::string pendingMFASessionInfo = "";
    std::string lastLoggedInUser = "";
    std::unordered_map<std::string, std::string> usernamesToEmails; // Map usernames to emails
    std::unordered_map<std::string, std::string> pendingVerificationCodes; // Map emails to verification codes
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> codeTimestamps; // Code expiration
    
    // Thread safety for Firebase operations
#ifdef FIREBASE_AVAILABLE
    std::vector<firebase::Future<firebase::database::DataSnapshot>> pendingFirebaseFutures;
#endif
    
    // Thread-safe engine state updates
    std::mutex engineUpdateMutex;
    std::string pendingEngineStateUpdate;
    bool hasPendingEngineUpdate = false;

    void initUI();
    void initUIResources();
    void createWindow();
    void loadComponents();
    void rebuildUI();
    void loadLayoutConfig();
    void toggleFullscreen();
    void cleanup();

    void handleEvents();
    void handleDragAndDrop();
    
    void scanAndLoadPlugins();
    bool loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void unloadAllPlugins();
    
    void addTrustedPlugin(const std::string& pluginName);
    void removeTrustedPlugin(const std::string& pluginName);
    void setPluginTrusted(const std::string& pluginName, bool trusted);
    
    bool isPluginTrusted(const std::string& pluginName) const;
    
    // Firebase resource cleanup
    void cleanupFirebaseResources();
    
    // Thread-safe engine updates
    void processPendingEngineUpdates();
};