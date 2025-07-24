#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
#include "TimelineHelpers.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

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
 * 
 * Owns all top-level UI elements and coordinates their updates with the engine.
 */
class Application {
public:
    Application();
    ~Application();

    void update();
    void render();

    bool isRunning() const;

private:
    sf::RenderWindow window;
    sf::View windowView;
    sf::VideoMode screenResolution;
    Engine engine;
    UILO* ui = nullptr;
    UIState uiState;
    UIResources resources;

    std::stack<std::string> undoStack;
    std::stack<std::string> redoStack;

    bool running = false;
    bool showMixer = false;
    bool playing = false;
    bool uiChanged = false;

    float timelineOffset = 0.f;

    // UI Components
    Row* topRowElement;
    Column* fileBrowserElement;
    Row* masterTrackElement;
    ScrollableColumn* timelineElement;
    ScrollableRow* mixerElement;
    Column* masterMixerTrackElement;
    Row* browserAndTimelineElement;
    Row* browserAndMixerElement;
    Row* fxRackElement;
    FreeColumn* contextMenu;

    // UI Component Definitions
    Row* topRow();
    Row* browserAndTimeline();
    Row* browserAndMixer();
    Row* masterTrack();
    Column* fileBrowser();
    ScrollableColumn* timeline();
    Row* fxRack();

    Row* track(
        const std::string& trackName = "", 
        Align alignment = Align::LEFT | Align::TOP, 
        float volume = 0.75f, 
        float pan = 0.0f
    );

    ScrollableRow* mixer();

    Column* mixerTrack(
        const std::string& trackName = "", 
        Align alignment = Align::LEFT | Align::TOP, 
        float volume = 0.75f, 
        float pan = 0.0f
    );
    
    Column* masterMixerTrack(
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
    
    // Timeline rendering helpers
    void updateTimelineRendering();
    float calculatePlayheadFollowOffset(float currentOffset);
    void renderTimelineElements(float clampedOffset);
    void setupTrackInteractionHandlers(Track* track, uilo::Element* trackRow, float clampedOffset);
    std::pair<std::vector<std::shared_ptr<sf::Drawable>>, std::vector<std::shared_ptr<sf::Drawable>>> 
        generateAndClipTrackElements(Track* track, uilo::Element* trackRow, ScrollableRow* scrollableRow, float clampedOffset);
    void clipAndPositionElement(std::shared_ptr<sf::RectangleShape> rect, float yOffset, float leftBound, float rightBound);
    void addPlayheadToTimeline(std::vector<std::shared_ptr<sf::Drawable>>& allTimelineElements, float clampedOffset);
    
    void undo();
    void redo();
    float getDistance(sf::Vector2f point1, sf::Vector2f point2);
};
