#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <SFML/System/Clock.hpp>
#include <string>
#include <stack>

using namespace uilo;

/**
 * @brief Main application class for the audio workstation.
 *
 * Handles:
 * - UI initialization and management using UILO.
 * - Audio engine state and project management.
 * - Undo/redo stack for project state.
 * - User input, event handling, and UI updates.
 * - File operations (load/save/select).
 * - Auto-save functionality loaded from and persisted to config.json.
 */
// namespace nlohmann { class json; }
using namespace uilo;

class Application {
public:
    Application();
    ~Application();

    void update();
    void render();
    void rebuildUI(); 
    bool isRunning() const;

private:
    // Window and rendering
    sf::RenderWindow window;
    sf::View          windowView;
    sf::VideoMode     screenResolution;
    std::string       currentPage = "timeline";
    std::string       pendingLoadPath;

    // Core engine and UI
    Engine   engine;
    UILO*    ui = nullptr;
    UIState  uiState;
    UIResources resources;

    // Undo/redo stacks
    std::stack<std::string> undoStack;
    std::stack<std::string> redoStack;

    // Application state flags
    bool running   = false;
    bool showMixer = false;
    bool playing   = false;
    bool uiChanged = false;

    // UI Components
    Row*             topRowElement;
    Column*          fileBrowserElement;
    Row*             masterTrackElement;
    ScrollableColumn* timelineElement;
    ScrollableRow*   mixerElement;
    Column*          masterMixerTrackElement;
    Row*             browserAndTimelineElement;
    Row*             browserAndMixerElement;
    Row*             fxRackElement;
    FreeColumn*      contextMenu;

    // Auto-save configuration
    int        autoSaveIntervalSeconds = 300;  // loaded from config.json (default 5m)
    sf::Clock  autoSaveTimer;                // tracks elapsed time since last save
    std::string lastSavePath;                // path for repeated auto-saves

    // Filename for configuration storage
    const std::string configFilePath = "config.json";

    // UI Component Definitions
    Row*                 topRow();
    Row*                 browserAndTimeline();
    Row*                 browserAndMixer();
    Row*                 masterTrack();
    Column*              fileBrowser();
    ScrollableColumn*    timeline();
    Row*                 fxRack();
    Row*                 track(
        const std::string& trackName = "",
        Align alignment = Align::LEFT | Align::TOP,
        float volume = 0.75f,
        float pan = 0.0f
    );
    ScrollableRow*       mixer();
    Column*              mixerTrack(
        const std::string& trackName = "",
        Align alignment = Align::LEFT | Align::TOP,
        float volume = 0.75f,
        float pan = 0.0f
    );
    Column*              masterMixerTrack(
        const std::string& trackName = "Master",
        Align alignment = Align::LEFT | Align::TOP,
        float volume = 0.75f,
        float pan = 0.0f
    );

    // UI Helpers
    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters = {"*.wav", "*.mp3", "*.flac"});
    void newTrack();
    void loadComposition(const std::string& path);
    void initUIResources();
    void rebuildUIFromEngine();
    bool handleTrackEvents();

    // Update function helpers
    bool handleContextMenu();
    bool handleUIButtons();
    bool handlePlaybackControls();
    bool handleKeyboardShortcuts();

    void undo();
    void redo();
    float getDistance(sf::Vector2f point1, sf::Vector2f point2);

    // Auto-save routines
    void loadConfig();   // read autoSaveIntervalSeconds from config.json
    void saveConfig();   // write current autoSaveIntervalSeconds to config.json
    void checkAutoSave(); // called within update()
};