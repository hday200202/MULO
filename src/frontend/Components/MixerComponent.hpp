#pragma once

#include "MULOComponent.hpp"
#include <unordered_map>

class MixerComponent : public MULOComponent {
public:
    MixerComponent();
    ~MixerComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;
    inline Container* getLayout() override { return layout; }

    void rebuildUI();
    void setMixerVisible(bool visible);
    bool isMixerVisible() const { return mixerShown; }
    
    // Override visibility methods to properly handle mixer state
    void show() override { setMixerVisible(true); }
    void hide() override { setMixerVisible(false); }
    bool isVisible() const override { return mixerShown; }

private:
    int displayedTrackCount = 0;
    bool shouldRebuild = false;
    bool mixerShown = false;
    bool wasVisible = false; // Track previous visibility state

    // UI Elements
    ScrollableRow* mixerScrollable = nullptr;
    Column* masterMixerTrackElement = nullptr;
    
    // Track UI element storage
    std::unordered_map<std::string, Column*> mixerTrackElements;
    std::unordered_map<std::string, Button*> soloButtons;
    std::unordered_map<std::string, Slider*> volumeSliders;
    std::unordered_map<std::string, Slider*> panSliders;

    // UI creation methods
    Column* createMixerTrack(const std::string& trackName, float volume = 1.0f, float pan = 0.5f);
    Column* createMasterMixerTrack();
    
    // UI rebuild helpers
    void rebuildUIFromEngine();
    void clearTrackElements();
    void syncSlidersToEngine();
    
    // Pan conversion helpers
    float enginePanToSlider(float enginePan) const { return (enginePan + 1.0f) * 0.5f; } // -1,+1 to 0,1
    float sliderPanToEngine(float sliderPan) const { return (sliderPan * 2.0f) - 1.0f; } // 0,1 to -1,+1
};