
#pragma once

#include "MULOComponent.hpp"

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
    void show() override { setMixerVisible(true); }
    void hide() override { setMixerVisible(false); }
    bool isVisible() const override { return mixerShown; }

private:
    int displayedTrackCount = 0;
    bool shouldRebuild = false;
    bool mixerShown = false;
    bool wasVisible = false;

    ScrollableRow* mixerScrollable = nullptr;
    Column* masterMixerTrackElement = nullptr;

    std::unordered_map<std::string, Column*> mixerTrackElements;
    std::unordered_map<std::string, Button*> soloButtons;
    std::unordered_map<std::string, Button*> muteButtons;
    std::unordered_map<std::string, Slider*> volumeSliders;
    std::unordered_map<std::string, Slider*> panSliders;
    std::unordered_map<std::string, bool> lastSoloButtonStates;
    std::unordered_map<std::string, bool> lastMuteButtonStates;

    Column* createMixerTrack(const std::string& trackName, float volume = 1.0f, float pan = 0.5f);
    Column* createMasterMixerTrack();
    void rebuildUIFromEngine();
    void clearTrackElements();
    void syncSlidersToEngine();

    float enginePanToSlider(float enginePan) const { return (enginePan + 1.0f) * 0.5f; }
    float sliderPanToEngine(float sliderPan) const { return (sliderPan * 2.0f) - 1.0f; }
};

#include "Application.hpp"

MixerComponent::MixerComponent() { 
    name = "mixer"; 
}

MixerComponent::~MixerComponent() {
    clearTrackElements();
}

void MixerComponent::init() {
    if (app->mainContentRow) {
        parentContainer = app->mainContentRow;
        mixerShown = false;
    }
    else
        return;
    
    // Create master mixer track
    masterMixerTrackElement = createMasterMixerTrack();
    
    // Create scrollable mixer for regular tracks
    mixerScrollable = scrollableRow(
        Modifier().setWidth(1.f).setHeight(1.f).setColor(app->resources.activeTheme->track_row_color),
        contains{}, 
        "mixer"
    );
    
    containers["mixer"] = mixerScrollable;
    
    // Build tracks from engine
    rebuildUIFromEngine();
    
    // Create the main layout (row containing master track and scrollable mixer)
    layout = row(
        Modifier().setWidth(1.f).setHeight(1.f).align(Align::RIGHT),
        contains{
            masterMixerTrackElement,
            mixerScrollable
        }
    );
    
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
        
        // Sync slider values to current engine state after initialization
        syncSlidersToEngine();
    }
}

void MixerComponent::update() {
    if (!initialized) return;
    
    // Check if we need to rebuild UI (track count changed)
    int currentTrackCount = app->getAllTracks().size();
    if (currentTrackCount != displayedTrackCount) {
        shouldRebuild = true;
        displayedTrackCount = currentTrackCount;
    }
    
    if (shouldRebuild) {
        rebuildUI();
        shouldRebuild = false;
        forceUpdate = true;
        return; // Don't update slider values during rebuild
    }

    // Handle "last shown wins" logic - if mixer becomes visible, hide piano roll
    if (mixerShown && !wasVisible) {
        // Mixer is being shown, hide piano roll and reset its layout
        if (auto* pianoRoll = app->getComponent("piano_roll")) {
            if (pianoRoll->getLayout()) {
                pianoRoll->getLayout()->m_modifier.setVisible(false);
                pianoRoll->getLayout()->m_modifier.setWidth(0.f);
            }
            pianoRoll->hide();
        }
        wasVisible = true;
    }
    else if (!mixerShown && wasVisible) {
        wasVisible = false;
    }

    // Handle timeline visibility - hide timeline when mixer is shown
    if (auto* timelineComponent = app->getComponent("timeline")) {
        if (timelineComponent->getLayout()) {
            if (mixerShown) {
                timelineComponent->getLayout()->m_modifier.setVisible(false);
                timelineComponent->getLayout()->m_modifier.setWidth(0.f);
                timelineComponent->hide();
            } else {
                // Only restore timeline if piano roll is also hidden
                if (auto* pianoRoll = app->getComponent("piano_roll")) {
                    if (!pianoRoll->isVisible()) {
                        timelineComponent->getLayout()->m_modifier.setVisible(true);
                        timelineComponent->getLayout()->m_modifier.setWidth(1.f);
                        timelineComponent->show();
                    }
                }
            }
        }
    }
}

bool MixerComponent::handleEvents() {
    if (!initialized) return false;
    
    bool forceUpdate = app->isPlaying();
    constexpr float tolerance = 0.001f;

    if (layout && mixerShown) {
        layout->m_modifier.setVisible(true);
        layout->m_modifier.setWidth(1.f);  // Ensure mixer gets full width when shown
        
        // Sync sliders to engine when component becomes visible
        if (!wasVisible) {
            syncSlidersToEngine();
        }
    }
    else if (layout && !mixerShown) {
        layout->m_modifier.setVisible(false);
        return false;
    }
    
    // Handle all track slider changes (including master track)
    std::vector<Track*> allTracksToProcess;
    
    // Add master track
    if (app->getMasterTrack()) {
        allTracksToProcess.push_back(app->getMasterTrack());
    }
    
    // Add regular tracks
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() != "Master") {
            allTracksToProcess.push_back(track.get());
        }
    }
    
    // Process all tracks with identical logic
    for (Track* track : allTracksToProcess) {
        if (!track) continue;

        const std::string& name = track->getName();
        auto volumeIt = volumeSliders.find(name);
        auto panIt = panSliders.find(name);
        auto soloIt = soloButtons.find(name);
        auto muteIt = muteButtons.find(name);

        // Handle solo button clicks with state tracking (rising edge)
        if (soloIt != soloButtons.end() && soloIt->second) {
            bool currentSoloState = soloIt->second->isClicked();
            bool& lastSoloState = lastSoloButtonStates[track->getName()];

            if (currentSoloState && !lastSoloState) {
                track->setSolo(!track->isSolo());
                soloIt->second->m_modifier.setColor(
                    track->isSolo() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->button_color
                );
                soloIt->second->setClicked(false); // Reset after processing
                forceUpdate = true;
            }
            lastSoloState = currentSoloState;
        }

        // Handle mute button clicks with state tracking (rising edge)
        if (muteIt != muteButtons.end() && muteIt->second) {
            bool currentMuteState = muteIt->second->isClicked();
            bool& lastMuteState = lastMuteButtonStates[track->getName()];

            if (currentMuteState && !lastMuteState) {
                track->toggleMute();
                muteIt->second->m_modifier.setColor(
                    track->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->button_color
                );
                muteIt->second->setClicked(false); // Reset after processing
                forceUpdate = true;
            }
            lastMuteState = currentMuteState;
        }

        // Handle volume slider changes
        if (volumeIt != volumeSliders.end() && volumeIt->second) {
            const float sliderDb = floatToDecibels(volumeIt->second->getValue());
            if (std::abs(track->getVolume() - sliderDb) > tolerance) {
                track->setVolume(sliderDb);
                forceUpdate = true;
            }
        }

        // Handle pan slider changes
        if (panIt != panSliders.end() && panIt->second) {
            float sliderValue = panIt->second->getValue(); // 0.0 to 1.0
            float enginePanValue = sliderPanToEngine(sliderValue); // Convert to -1.0 to +1.0
            if (std::abs(track->getPan() - enginePanValue) > tolerance) {
                track->setPan(enginePanValue);
                forceUpdate = true;
            }
        }
    }
    
    return forceUpdate;
}

void MixerComponent::rebuildUI() {
    rebuildUIFromEngine();
}

void MixerComponent::setMixerVisible(bool visible) {
    mixerShown = visible;
    if (layout) layout->m_modifier.setVisible(visible);
    
    // Also toggle timeline visibility
    if (auto* timelineComponent = app->getComponent("timeline")) {
        timelineComponent->setVisible(!visible);
    }
    
    forceUpdate = true; // Force an update to apply visibility changes immediately
}

Column* MixerComponent::createMixerTrack(const std::string& trackName, float volume, float pan) {
    volumeSliders[trackName] = slider(
        Modifier().setfixedWidth(32).setHeight(1.f).align(Align::CENTER_X | Align::BOTTOM),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        trackName + "_mixer_volume_slider"
    );

    panSliders[trackName] = slider(
        Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Horizontal,
        trackName + "_mixer_pan_slider"
    );

    soloButtons[trackName] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(app->resources.activeTheme->button_color),
        ButtonStyle::Rect,
        "solo",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "solo_" + trackName
    );

    muteButtons[trackName] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(app->resources.activeTheme->button_color),
        ButtonStyle::Rect,
        "mute",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_" + trackName
    );

    auto mixerTrack = column(
        Modifier()
            .setColor(app->resources.activeTheme->track_color)
            .setfixedWidth(96)
            .align(Align::LEFT),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
            text(
                Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
                trackName,
                app->resources.dejavuSansFont
            ),

            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),
            volumeSliders[trackName],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            soloButtons[trackName],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            muteButtons[trackName],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
            contains{panSliders[trackName],}),
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
        }
    );

    return mixerTrack;
}

Column* MixerComponent::createMasterMixerTrack() {
    volumeSliders["Master"] = slider(
        Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        "Master_mixer_volume_slider"
    );

    panSliders["Master"] = slider(
        Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Horizontal,
        "Master_mixer_pan_slider"
    );

    soloButtons["Master"] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(app->resources.activeTheme->button_color),
        ButtonStyle::Rect,
        "solo",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "solo_Master"
    );

    muteButtons["Master"] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(app->resources.activeTheme->button_color),
        ButtonStyle::Rect,
        "mute",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_Master"
    );

    auto masterTrack = column(
        Modifier()
            .setColor(app->resources.activeTheme->master_track_color)
            .setfixedWidth(96)
            .align(Align::LEFT)
            .setHighPriority(true),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
            text(
                Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
                "Master",
                app->resources.dejavuSansFont
            ),

            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),
            volumeSliders["Master"],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            soloButtons["Master"],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            muteButtons["Master"],
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

            row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
            contains{ panSliders["Master"],}),
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
        }
    );

    return masterTrack;
}

void MixerComponent::rebuildUIFromEngine() {
    if (!mixerScrollable) return;
    
    // Clear existing track elements
    clearTrackElements();
    mixerScrollable->clear();
    
    // Add tracks from engine (skip Master track)
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        auto mixerTrackElement = createMixerTrack(track->getName(), track->getVolume(), track->getPan());
        mixerTrackElements[track->getName()] = mixerTrackElement;
        mixerScrollable->addElement(mixerTrackElement);
    }
    
    // Set scroll speed
    mixerScrollable->setScrollSpeed(20.f);
    
    displayedTrackCount = app->getAllTracks().size();
    
    // Sync slider values to engine after UI is built
    syncSlidersToEngine();
}

void MixerComponent::clearTrackElements() {
    mixerTrackElements.clear();
    
    // Clear regular track UI elements but preserve master track elements
    auto masterSolo = soloButtons.find("Master");
    auto masterVolume = volumeSliders.find("Master");
    auto masterPan = panSliders.find("Master");
    
    // Save master track elements
    Button* savedMasterSolo = (masterSolo != soloButtons.end()) ? masterSolo->second : nullptr;
    Slider* savedMasterVolume = (masterVolume != volumeSliders.end()) ? masterVolume->second : nullptr;
    Slider* savedMasterPan = (masterPan != panSliders.end()) ? masterPan->second : nullptr;
    
    // Clear all
    soloButtons.clear();
    volumeSliders.clear();
    panSliders.clear();
    
    // Restore master track elements
    if (savedMasterSolo) soloButtons["Master"] = savedMasterSolo;
    if (savedMasterVolume) volumeSliders["Master"] = savedMasterVolume;
    if (savedMasterPan) panSliders["Master"] = savedMasterPan;
}

void MixerComponent::syncSlidersToEngine() {
    // Process all tracks with unified logic (including master track)
    std::vector<Track*> allTracksToSync;
    
    // Add master track
    if (app->getMasterTrack()) {
        allTracksToSync.push_back(app->getMasterTrack());
    }
    
    // Add regular tracks
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() != "Master") {
            allTracksToSync.push_back(track.get());
        }
    }
    
    // Sync all tracks with identical logic
    for (Track* track : allTracksToSync) {
        if (!track) continue;
        
        const std::string& trackName = track->getName();
        auto volumeSlider = volumeSliders.find(trackName);
        auto panSlider = panSliders.find(trackName);
        
        if (volumeSlider != volumeSliders.end() && volumeSlider->second) {
            float engineVol = track->getVolume();
            float sliderValue = decibelsToFloat(engineVol);
            volumeSlider->second->setValue(sliderValue);
        }
        
        if (panSlider != panSliders.end() && panSlider->second) {
            float enginePan = track->getPan(); // -1 to +1
            float sliderValue = enginePanToSlider(enginePan); // Convert to 0 to 1
            panSlider->second->setValue(sliderValue);
        }
    }
}

// Plugin interface for MixerComponent
GET_INTERFACE

// Plugin interface implementation
DECLARE_PLUGIN(MixerComponent)
