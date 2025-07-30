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
    bool wasVisible = true; // Timeline starts visible by default

    // Delta time for frame-rate independent scrolling
    std::chrono::steady_clock::time_point lastFrameTime;
    float deltaTime = 0.0f;
    bool firstFrame = true;

    // Cached measure lines optimization
    std::vector<std::shared_ptr<sf::Drawable>> cachedMeasureLines;
    float lastMeasureWidth = -1.f;
    float lastScrollOffset = -1.f;
    sf::Vector2f lastRowSize = {-1.f, -1.f};

    // Cached clip geometry optimization
    std::unordered_map<std::string, std::vector<std::shared_ptr<sf::Drawable>>> cachedClipGeometry;
    std::unordered_map<std::string, size_t> lastClipCount;
    std::unordered_map<std::string, float> lastClipBeatWidth;
    std::unordered_map<std::string, float> lastClipScrollOffset;
    std::unordered_map<std::string, sf::Vector2f> lastClipRowSize;
    std::unordered_map<std::string, std::string> lastSelectedTrack;

    Row* masterTrackElement = nullptr;
    Button* muteMasterButton = nullptr;
    Slider* masterVolumeSlider = nullptr;

    std::unordered_map<std::string, Button*> trackMuteButtons;
    std::unordered_map<std::string, Slider*> trackVolumeSliders;

    Row* masterTrack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);

    void handleCustomUIElements();
    void rebuildUIFromEngine();
    void syncSlidersToEngine();
    
    // Cached measure lines optimization
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedMeasureLines(
        float measureWidth, float scrollOffset, const sf::Vector2f& rowSize);
    
    // Cached clip geometry optimization
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedClipGeometry(
        const std::string& trackName, double bpm, float beatWidth, float scrollOffset, 
        const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, 
        UIResources* resources, UIState* uiState, const AudioClip* selectedClip,
        const std::string& currentTrackName, const std::string& selectedTrackName);
    
    // Pan conversion helpers (same as MixerComponent for consistency)
    float enginePanToSlider(float enginePan) const { return (enginePan + 1.0f) * 0.5f; } // -1,+1 to 0,1
    float sliderPanToEngine(float sliderPan) const { return (sliderPan * 2.0f) - 1.0f; } // 0,1 to -1,+1
};