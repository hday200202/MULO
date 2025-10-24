#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "Data/UIData.hpp"
#include "Util/FileTree.hpp"
#include "Extension/MULOComponent.hpp"
#include "PlatformDefines.hpp"
#include "Firebase-User/EmailService.hpp"
#include "Config/GlobalSettings.hpp"
#include <nlohmann/json.hpp>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <chrono>
#include <list>

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
    std::unique_ptr<GlobalSettings> globalSettings = nullptr;


    const juce::String getApplicationName() override { return "MULO"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override;
    void shutdown() override;

    Application();
    ~Application();

    void update();
    void render();
    bool isRunning() const;

    Container* getComponentLayout(const std::string& componentName);
    MULOComponent* getComponent(const std::string& componentName);
    Container* getPageBaseContainer();
    Row* getMainContentRow();
    void setComponentParentContainer(const std::string& componentName, Container* parent);

    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters);

    const sf::RenderWindow& getWindow() const;
    void requestUIRebuild();
    void requestFullscreenToggle();
    void requestThemeChange(const std::string& themeName);

    void openColorPicker(sf::Vector2f position);
    void handleColorPicker();
    void renderColorPicker();
    const sf::Color& getRecentColorPicked() const;
    bool isColorPickerOpen() const;
    bool wasColorSelected() const;
    void closeColorPicker();

    Track* getMasterTrack();
    Track* getTrack(const std::string& name);
    std::vector<std::unique_ptr<Track>>& getAllTracks();
    void addTrack(const std::string& name, const std::string& samplePath);
    void removeTrack(const std::string& name);
    void exportAudio();
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const;

    void playSound(const std::string& filePath, float db);
    void playSound(const juce::File& file, float db);

    std::string getEngineStateString() const;
    void loadEngineStateString(const std::string& stateString);
    std::string getEngineStateHash() const;

    void sendMIDINote(int noteNumber, int velocity, bool noteOn = true);

    void addEffect(const std::string& filePath);
    void addSynthesizer(const std::string& filePath);
    void requestOpenEffectWindow(size_t effectIndex);
    void deferEffectLoading(const std::string& trackName, const std::string& vstPath, bool openWindow = false, bool enabled = true, int index = -1, const std::vector<std::pair<int, float>>& parameters = {});

    void play();
    void pause();
    void setSavedPosition(double seconds);
    bool isPlaying() const;
    void setBpm(float bpm);
    float getBpm() const;
    double getPosition() const;
    double getSavedPosition() const;
    void setPosition(double seconds);
    std::pair<int, int> getTimeSignature();

    AudioClip* getReferenceClip(const std::string& trackName);
    void addClipToTrack(const std::string& trackName, const AudioClip& clip);
    void removeClipFromTrack(const std::string& trackName, size_t index);
    void updateClipInTrack(const std::string& trackName, size_t index, const AudioClip& newClip);

    double getSampleRate() const;
    void setSampleRate(const double newSampleRate);

    template<typename T>
    void writeConfig(const std::string& key, const T& value);

    template<typename T>
    T readConfig(const std::string& key, const T& defaultValue = T{}) const;

    void saveConfig();
    void loadConfig();
    void syncUIStateToConfig();
    void saveLayoutConfig();
    void loadLayoutConfig();

    void setSelectedTrack(const std::string& trackName);
    std::string getSelectedTrack() const;
    Track* getSelectedTrackPtr();
    bool hasSelectedTrack() const;

    void loadComposition(const std::string& path);
    std::string getCurrentCompositionName() const;
    void setCurrentCompositionName(const std::string& name);
    void saveState();
    void saveToFile(const std::string& path) const;

    MIDIClip* getSelectedMIDIClip() const;
    MIDIClip* getTimelineSelectedMIDIClip() const;

    void updateParameterTracking();

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
    void uploadExtension(const ExtensionData& extensionData, const std::vector<std::string>& filePaths, 
                        std::function<void(FirebaseState, const std::string&)> callback);
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
    firebase::storage::Storage* storage = nullptr;
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
    std::unordered_map<std::string, std::string> usernamesToEmails;
    std::unordered_map<std::string, std::string> pendingVerificationCodes;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> codeTimestamps;
    
    std::string lastWindowTitle = "";
    
#ifdef FIREBASE_AVAILABLE
    std::vector<firebase::Future<firebase::database::DataSnapshot>> pendingFirebaseFutures;
#endif
    
    std::mutex engineUpdateMutex;
    std::string pendingEngineStateUpdate;
    bool hasPendingEngineUpdate = false;
    // Theme change is applied from the main loop to avoid re-entrant UI/resource updates
    bool hasPendingThemeChange = false;
    std::string pendingThemeName;

    sf::Clock logoPageTimer;
    std::string currentPage = "";

    // Color picker members
    sf::RenderWindow colorPickerWindow;
    std::unique_ptr<UILO> colorPickerUI;
    uilo::Grid* colorPickerGrid = nullptr;
    sf::Color recentColorPicked = sf::Color::White;
    bool colorPickerOpen = false;
    bool colorWasSelected = false;

    void initUI();
    void initUIResources();
    void createWindow();
    void loadComponents();
    void rebuildUI();
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
    std::string getAlignmentString(uilo::Align alignment) const;
    
    void cleanupFirebaseResources();    
    void processPendingEngineUpdates();

    void createLogoScreen();
};