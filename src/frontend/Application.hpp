#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "TimelineHelpers.hpp"
#include "FileTree.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <stack>
#include <chrono>
#include <map>
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
    // Core systems
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
    FileTree fileTree;

    // Application state
    bool running = false;
    bool showMixer = false;
    bool showSettings = false;
    bool showThemeDropdown = false;
    bool showSampleRateDropdown = false;
    bool textInputActive = false;
    bool projectNameInputActive = false;
    bool bpmInputActive = false;
    bool playing = false;
    bool uiChanged = false;
    bool fileTreeNeedsRebuild = false;
    float timelineOffset = 0.f;
    std::string textInputValue = "";
    std::string projectNameValue = "untitled";
    std::string bpmValue = "120";

    // Tooltip timing
    sf::Clock toolTipTimer;
    std::string currentHoveredButton = "";
    bool tooltipShown = false;

    // Undo/Redo stacks
    std::stack<std::string> undoStack;
    std::stack<std::string> redoStack;

    // UI Components
    Row* topRowElement;
    ScrollableColumn* fileBrowserElement;
    Row* masterTrackElement;
    ScrollableColumn* timelineElement;
    ScrollableRow* mixerElement;
    Column* masterMixerTrackElement;
    Row* browserAndTimelineElement;
    Row* browserAndMixerElement;
    Row* fxRackElement;
    FreeColumn* contextMenu;
    FreeColumn* toolTip;
    ScrollableColumn* settingsColumnElement;
    FreeColumn* dropdownMenu;
    FreeColumn* sampleRateDropdownMenu;

    // Auto-save configuration
    int        autoSaveIntervalSeconds = 300;  // loaded from config.json (default 5m)
    sf::Clock  autoSaveTimer;                // tracks elapsed time since last save
    std::string lastSavePath;                // path for repeated auto-saves

    // Filename for configuration storage
    const std::string configFilePath = "config.json";

    // Initialization methods
    void initWindow();
    void initUIResources();
    void initUI();
    
    // Timeline update methods
    void updateTimeline();
    void updatePlayheadFollow(float& newMasterOffset);
    void updateTrackInteractions(float clampedOffset);

    // Input handling
    bool handleContextMenu();
    bool handleUIButtons();
    bool handlePlaybackControls();
    bool handleKeyboardShortcuts();
    bool handleScrollWheel();
    bool handleTrackEvents();
    bool handleToolTips();

    // UI creation methods
    Row* topRow();
    Row* browserAndTimeline();
    Row* browserAndMixer();
    ScrollableColumn* fileBrowser();
    ScrollableColumn* timeline();
    Row* fxRack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);
    Column* mixerTrack(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);
    Column* masterMixerTrack(const std::string& trackName = "Master", Align alignment = Align::LEFT, float volume = 1.0f, float pan = 0.0f);
    Row* masterTrack();
    ScrollableRow* mixer();
    ScrollableColumn* settingsColumn();
    FreeColumn* generateDropdown(sf::Vector2f position, const std::vector<std::string>& items);
    FreeColumn* generateSampleRateDropdown(sf::Vector2f position, const std::vector<std::string>& items);

    // File operations
    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters);
    void newTrack(const std::string& samplePath);
    void loadComposition(const std::string& path);
    void rebuildUIFromEngine();
    void buildFileTreeUI();
    void buildFileTreeUIRecursive(const FileTree& tree, int indentLevel);
    void updateFileBrowserUI();

    // Undo/Redo
    void undo();
    void redo();

    // Helper functions
    float getDistance(sf::Vector2f point1, sf::Vector2f point2);
    void toggleTreeNodeByPath(const std::string& path);

    // Auto-save routines
    void loadConfig();   // read autoSaveIntervalSeconds from config.json
    void saveConfig();   // write current autoSaveIntervalSeconds to config.json
    void checkAutoSave(); // called within update()
};