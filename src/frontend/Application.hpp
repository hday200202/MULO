#pragma once

#include "UILO/UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"
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

    // UI Components
    Row* topRowElement;
    Column* fileBrowserElement;
    Row* masterTrackElement;
    Column* timelineElement;
    Row* mixerElement;
    Column* masterMixerTrackElement;
    Row* browserAndTimelineElement;
    Row* browserAndMixerElement;
    Row* fxRackElement;

    // UI Component Definitions
    Row* topRow();
    Row* browserAndTimeline();
    Row* browserAndMixer();
    Row* masterTrack();
    Column* fileBrowser();
    Column* timeline();
    Row* fxRack();
    Row* track(const std::string& trackName = "", Align alignment = Align::LEFT | Align::TOP, float volume = 0.75f, float pan = 0.0f);
    Row* mixer();
    Column* mixerTrack(const std::string& trackName = "", Align alignment = Align::LEFT | Align::TOP, float volume = 0.75f, float pan = 0.0f);

    // UI Helpers
    std::string selectDirectory();
    std::string selectFile(std::initializer_list<std::string> filters = {"*.wav", "*.mp3", "*.flac"});
    void newTrack();
    void loadComposition(const std::string& path);
    void initUIResources();
    void rebuildUIFromEngine();
    void handleTrackEvents();
    void undo();
    void redo();
};
