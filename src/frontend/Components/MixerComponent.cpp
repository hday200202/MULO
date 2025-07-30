#include "MixerComponent.hpp"
#include "Application.hpp"
#include "../audio/Engine.hpp"
#include <iostream>

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
        Modifier().setWidth(1.f).setHeight(1.f).setColor(resources->activeTheme->track_row_color),
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
    if (!initialized || !engine) return;
    
    // Check if we need to rebuild UI (track count changed)
    int currentTrackCount = engine->getAllTracks().size();
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
}

bool MixerComponent::handleEvents() {
    if (!engine || !initialized) return false;
    
    bool forceUpdate = engine->isPlaying();
    constexpr float tolerance = 0.001f;

    if (layout && mixerShown) {
        layout->m_modifier.setVisible(true);
        if (auto* timelineComponent = app->getComponent("timeline")) {
            timelineComponent->hide();
        }
        
        // Sync sliders to engine when component becomes visible
        if (!wasVisible) {
            syncSlidersToEngine();
            wasVisible = true;
        }
    }
    else if (layout && !mixerShown) {
        layout->m_modifier.setVisible(false);
        if (auto* timelineComponent = app->getComponent("timeline")) {
            timelineComponent->show();
        }
        wasVisible = false;
        return false;
    }
    
    // Handle all track slider changes (including master track)
    std::vector<Track*> allTracksToProcess;
    
    // Add master track
    if (engine->getMasterTrack()) {
        allTracksToProcess.push_back(engine->getMasterTrack());
    }
    
    // Add regular tracks
    for (const auto& track : engine->getAllTracks()) {
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
        
        // Handle solo button clicks
        if (soloIt != soloButtons.end() && soloIt->second && soloIt->second->isClicked()) {
            track->setSolo(!track->isSolo());
            soloIt->second->m_modifier.setColor(
                track->isSolo() ? resources->activeTheme->mute_color : resources->activeTheme->button_color
            );
            forceUpdate = true;
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
        resources->activeTheme->slider_knob_color,
        resources->activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        trackName + "_mixer_volume_slider"
    );

    panSliders[trackName] = slider(
        Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        resources->activeTheme->slider_knob_color,
        resources->activeTheme->slider_bar_color,
        SliderOrientation::Horizontal,
        trackName + "_mixer_pan_slider"
    );

    soloButtons[trackName] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(resources->activeTheme->button_color),
        ButtonStyle::Rect,
        "solo",
        resources->dejavuSansFont,
        resources->activeTheme->secondary_text_color,
        "solo_" + trackName
    );

    auto mixerTrack = column(
        Modifier()
            .setColor(resources->activeTheme->track_color)
            .setfixedWidth(96)
            .align(Align::LEFT),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
            text(
                Modifier().setColor(resources->activeTheme->primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
                trackName,
                resources->dejavuSansFont
            ),

            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            volumeSliders[trackName],

            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

            row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
            contains{
                panSliders[trackName],
            }),

            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

            soloButtons[trackName],
        }
    );

    return mixerTrack;
}

Column* MixerComponent::createMasterMixerTrack() {
    volumeSliders["Master"] = slider(
        Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
        resources->activeTheme->slider_knob_color,
        resources->activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        "Master_mixer_volume_slider"
    );

    panSliders["Master"] = slider(
        Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        resources->activeTheme->slider_knob_color,
        resources->activeTheme->slider_bar_color,
        SliderOrientation::Horizontal,
        "Master_mixer_pan_slider"
    );

    soloButtons["Master"] = button(
        Modifier()
            .setfixedHeight(32)
            .setfixedWidth(64)
            .align(Align::CENTER_X | Align::BOTTOM)
            .setColor(resources->activeTheme->button_color),
        ButtonStyle::Rect,
        "solo",
        resources->dejavuSansFont,
        resources->activeTheme->secondary_text_color,
        "solo_Master"
    );

    auto masterTrack = column(
        Modifier()
            .setColor(resources->activeTheme->master_track_color)
            .setfixedWidth(96)
            .align(Align::LEFT)
            .setHighPriority(true),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
            text(
                Modifier().setColor(resources->activeTheme->primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
                "Master",
                resources->dejavuSansFont
            ),

            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            volumeSliders["Master"],
            
            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

            row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
            contains{
                panSliders["Master"],
            }),

            spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

            soloButtons["Master"],
        }
    );

    return masterTrack;
}

void MixerComponent::rebuildUIFromEngine() {
    if (!engine || !mixerScrollable) return;
    
    // Clear existing track elements
    clearTrackElements();
    mixerScrollable->clear();
    
    // Add tracks from engine (skip Master track)
    for (const auto& track : engine->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        auto mixerTrackElement = createMixerTrack(track->getName(), track->getVolume(), track->getPan());
        mixerTrackElements[track->getName()] = mixerTrackElement;
        mixerScrollable->addElement(mixerTrackElement);
    }
    
    // Set scroll speed
    mixerScrollable->setScrollSpeed(20.f);
    
    displayedTrackCount = engine->getAllTracks().size();
    
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
    if (!engine) return;
    
    // Process all tracks with unified logic (including master track)
    std::vector<Track*> allTracksToSync;
    
    // Add master track
    if (engine->getMasterTrack()) {
        allTracksToSync.push_back(engine->getMasterTrack());
    }
    
    // Add regular tracks
    for (const auto& track : engine->getAllTracks()) {
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
