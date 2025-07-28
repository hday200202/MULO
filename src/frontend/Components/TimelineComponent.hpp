#pragma once

#include "MULOComponent.hpp"
#include <chrono>

class AudioClip;

class TimelineComponent : public MULOComponent {
public:
    AudioClip* selectedClip = nullptr;

    TimelineComponent();
    ~TimelineComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;
    inline Container* getLayout() override { return layout; }

    void rebuildUI();

private:
    int displayedTrackCount = 0;
    float timelineOffset = 0.f;
    std::string selectedTrack = "";

    // Delta time for frame-rate independent scrolling
    std::chrono::steady_clock::time_point lastFrameTime;
    float deltaTime = 0.0f;
    bool firstFrame = true;

    Row* masterTrackElement = nullptr;
    Button* muteMasterButton = nullptr;
    Slider* masterVolumeSlider = nullptr;

    std::unordered_map<std::string, Button*> trackMuteButtons;
    std::unordered_map<std::string, Slider*> trackVolumeSliders;

    Row* masterTrack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);

    void handleCustomUIElements();
    void rebuildUIFromEngine();
};