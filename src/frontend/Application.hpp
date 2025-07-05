#pragma once

#include "UILO.hpp"
#include "Engine.hpp"
#include "UIData.hpp"

using namespace uilo;

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

    std::stack<UIState> undoStack;
    std::stack<UIState> redoStack;

    bool running = false;
    bool showMixer = false;
    bool playing = false;

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

    void handleTrackEvents();

    void undo();
    void redo();
};
