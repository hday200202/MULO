#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "TimelineHelpers.hpp"
#include "FileTree.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <stack>

using namespace uilo;

class Application {
public:
    Application();
    ~Application();

    void update();
    void render();
    bool isRunning() const;

private:
    // Core systems
    sf::RenderWindow window;
    sf::View windowView;
    sf::VideoMode screenResolution;
    Engine engine;
    UILO* ui = nullptr;
    UIState uiState;
    UIResources resources;
    FileTree fileTree;

    // Application state
    bool running = false;
    bool showMixer = false;
    bool playing = false;
    bool uiChanged = false;
    bool fileTreeNeedsRebuild = false;
    float timelineOffset = 0.f;

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
};
