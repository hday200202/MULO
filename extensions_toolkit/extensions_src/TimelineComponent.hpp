#pragma once

#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"
#include <chrono>
#include <set>
#include <unordered_map>
#include <cmath>

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

<<<<<<< Updated upstream
=======
    static bool clipEndDrag = false;
    static bool clipStartDrag = false;

    // Override to provide access to the selected MIDI clip
    MIDIClip* getSelectedMIDIClip() const override { 
        if (!selectedMIDIClipInfo.hasSelection) {
            return nullptr;
        }
        
        auto* track = app->getTrack(selectedMIDIClipInfo.trackName);
        if (!track || track->getType() != Track::TrackType::MIDI) {
            return nullptr;
        }
        
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        const auto& midiClips = midiTrack->getMIDIClips();
        
        for (auto& clip : midiClips) {
            if (std::abs(clip.startTime - selectedMIDIClipInfo.startTime) < 0.001 &&
                std::abs(clip.duration - selectedMIDIClipInfo.duration) < 0.001) {
                return const_cast<MIDIClip*>(&clip);
            }
        }
        
        return nullptr;
    }

    struct FeatureFlags {
        bool enableMouseInput = true;
        bool enableKeyboardInput = true;
        bool enableClipDragging = true;
        bool enableClipPlacement = true;
        bool enableClipDeletion = true;
        bool enableAutoFollow = true;
        bool enableVirtualCursor = true;
        bool enableWaveforms = true;
        bool enableUISync = true;
    } features;

>>>>>>> Stashed changes
private:
    float timelineOffset = 0.f;
    bool wasVisible = true;

    std::chrono::steady_clock::time_point lastFrameTime;
    float deltaTime = 0.0f;
    bool firstFrame = true;
    
    float lastScrubberPosition = 0.0f;
    bool scrubberPositionChanged = false;
    float expectedTimelineOffset = 0.0f;

    std::vector<std::shared_ptr<sf::Drawable>> cachedMeasureLines;
    float lastMeasureWidth = -1.f;
    float lastScrollOffset = -1.f;
    sf::Vector2f lastRowSize = {-1.f, -1.f};

    Row* masterTrackElement = nullptr;
    Button* muteMasterButton = nullptr;
    Slider* masterVolumeSlider = nullptr;

    std::unordered_map<std::string, Button*> trackMuteButtons;
    std::unordered_map<std::string, Slider*> trackVolumeSliders;
    std::unordered_map<std::string, Button*> trackSoloButtons;
    std::unordered_map<std::string, Button*> trackRemoveButtons;

    Row* masterTrack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);

    void handleCustomUIElements();
    void rebuildUIFromEngine();
    void syncSlidersToEngine();
    
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedMeasureLines(float measureWidth, float scrollOffset, const sf::Vector2f& rowSize);
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedClipGeometry(const std::string& trackName, double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, UIResources* resources, UIState* uiState, const AudioClip* selectedClip, const std::string& currentTrackName, const std::string& selectedTrackName);
    
    float enginePanToSlider(float enginePan) const { return (enginePan + 1.0f) * 0.5f; }
    float sliderPanToEngine(float sliderPan) const { return (sliderPan * 2.0f) - 1.0f; }
};

inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, UIResources* resources, UIState* uiState, const AudioClip* selectedClip, const std::string& currentTrackName, const std::string& selectedTrackName);

inline std::shared_ptr<sf::Drawable> getPlayHead(double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize
);

inline float getNearestMeasureX(
    const sf::Vector2f& pos, const std::vector<std::shared_ptr<sf::Drawable>>& lines
);

inline float secondsToXPosition(double bpm, float beatWidth, float seconds) noexcept;

inline std::vector<std::shared_ptr<sf::Drawable>> generateWaveformData(
    const AudioClip& clip, const sf::Vector2f& clipPosition, 
    const sf::Vector2f& clipSize, float verticalOffset, 
    UIResources* resources, UIState* uiState
);

inline std::vector<std::shared_ptr<sf::Drawable>> generateTimelineMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator,
    unsigned int sigDenominator,
    UIResources* resources
);

inline float xPosToSeconds(double bpm, float beatWidth, float xPos, float scrollOffset) noexcept;
inline std::unordered_map<std::string, std::vector<float>>& getWaveformCache();
inline void ensureWaveformIsCached(const AudioClip& clip);
inline void clearWaveformCache();

TimelineComponent::TimelineComponent() { 
    name = "timeline"; 
    selectedClip = nullptr; 
    lastFrameTime = std::chrono::steady_clock::now();
    deltaTime = 0.0f;
    firstFrame = true;
}

TimelineComponent::~TimelineComponent() {
    selectedClip = nullptr;
    app->writeConfig("scrubber_position", 0.f);
}

void TimelineComponent::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;

    relativeTo = "file_browser";
    masterTrackElement = masterTrack();

    ScrollableColumn* timelineScrollable = scrollableColumn(
        Modifier(),
        contains{}, "timeline"
    );

    containers["timeline"] = timelineScrollable;

    for (const auto& t : app->getAllTracks()) {
        if (t->getName() == "Master") continue;
        auto* trackRowElem = track(t->getName(), Align::TOP | Align::LEFT, t->getVolume(), t->getPan());
        timelineScrollable->addElement(spacer(Modifier().setfixedHeight(4.f)));
        timelineScrollable->addElement(trackRowElem);
        // Register the scrollable row for custom drawables
        if (trackRowElem) {
            auto& elements = trackRowElem->getElements();
            if (!elements.empty() && elements[0])
                containers[t->getName() + "_scrollable_row"] = static_cast<uilo::Container*>(elements[0]);
        }
    }

    layout = column(
        Modifier()
            .align(Align::RIGHT), 
        contains{
            column(Modifier().setColor(app->resources.activeTheme->middle_color).align(Align::RIGHT | Align::BOTTOM), contains{
            timelineScrollable,
            masterTrackElement
        }, "base_timeline_column")
    });

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void TimelineComponent::update() {
    if (!this->isVisible()) return; // Corrected isVisible() check
    
    auto currentTime = std::chrono::steady_clock::now();
    
    if (!firstFrame) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastFrameTime);
        deltaTime = duration.count() / 1000000.0f;
        
        constexpr float maxDeltaTime = 1.0f / 30.0f;
        deltaTime = std::min(deltaTime, maxDeltaTime);
    } else {
        deltaTime = 1.0f / 60.0f;
        firstFrame = false;
    }
    
    lastFrameTime = currentTime;
    
    handleCustomUIElements();

    // Check if scrubber position has changed
    float scrubberPos = app->readConfig<float>("scrubber_position", 0.0f);
    scrubberPositionChanged = (std::abs(scrubberPos - lastScrubberPosition) > 0.001f);
    
    float mousePosSeconds = xPosToSeconds(
        app->getBpm(),
        100.f * app->uiState.timelineZoomLevel,
        app->ui->getMousePosition().x,
        timelineState.timelineOffset
    );
    float selectedClipEnd = selectedClip->startTime + selectedClip->duration;
    float originalClipStartTime = 0.f;
    
    // Calculate the last clip position in the timeline
    double lastClipEndSeconds = 0.0;
    for (const auto& track : app->getAllTracks()) {
        const auto& clips = track->getClips();
        for (const auto& clip : clips) {
            double clipEndTime = clip.startTime + clip.duration;
            lastClipEndSeconds = std::max(lastClipEndSeconds, clipEndTime);
        }
    }
    
    // Minimum duration for when there are no clips
    if (lastClipEndSeconds <= 0.0) {
        lastClipEndSeconds = 1.0;
    }

    // Check for manual timeline scrolling first
    bool timelineWasManuallyScrolled = false;
    float currentTimelineOffset = timelineOffset;
    
    for (const auto& track : app->getAllTracks()) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
            float actualOffset = scrollableRow->getOffset();
            float diff = std::abs(actualOffset - expectedTimelineOffset);
            
            if (diff > 0.01f) {
                currentTimelineOffset = actualOffset;
                timelineOffset = actualOffset;
                timelineWasManuallyScrolled = true;
                break;
            }
        }
    }

    // Apply scrubber-to-timeline sync if scrubber moved and timeline wasn't manually scrolled
    if (scrubberPositionChanged && !timelineWasManuallyScrolled) {
        double scrubberTimeSeconds = scrubberPos * lastClipEndSeconds;
        float beatWidth = 100.f * app->uiState.timelineZoomLevel;
        float scrubberPixelPos = secondsToXPosition(app->getBpm(), beatWidth, scrubberTimeSeconds);
        
        timelineOffset = -scrubberPixelPos;
        expectedTimelineOffset = timelineOffset;

        for (const auto& track : app->getAllTracks()) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                scrollableRow->setOffset(std::min(0.f, timelineOffset));
            }
        }
        
        lastScrubberPosition = scrubberPos;
    }
    
    // Update scrubber position if timeline was manually scrolled
    if (timelineWasManuallyScrolled) {
        float beatWidth = 100.f * app->uiState.timelineZoomLevel;
        double currentTimeSeconds = xPosToSeconds(app->getBpm(), beatWidth, -currentTimelineOffset, 0.0f);
        
        currentTimeSeconds = std::max(0.0, std::min(currentTimeSeconds, lastClipEndSeconds));
        float newScrubberPos = lastClipEndSeconds > 0.0 ? static_cast<float>(currentTimeSeconds / lastClipEndSeconds) : 0.0f;
        
        app->writeConfig("scrubber_position", newScrubberPos);
        lastScrubberPosition = newScrubberPos;
        
        timelineOffset = currentTimelineOffset;
        expectedTimelineOffset = timelineOffset;
        
        for (const auto& track : app->getAllTracks()) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                scrollableRow->setOffset(timelineOffset);
            }
        }
    }

    if(mousePosSeconds <= selectedClipEnd + 5.0 && mousePosSeconds >= selectedClipEnd) {
        if(app->ui->isMouseDragging()) {
            clipEndDrag = true;
        }
    }

    if(clipEndDrag) {
        if(mousePosSeconds < selectedClipEnd) {
            selectedClip->duration = mousePosSeconds - selectedClip->startTime;
        }
    }

    if(mousePosSeconds >= selectedClipEnd + 5.0 && mousePosSeconds <= selectedClipEnd) {
        if(app->ui->isMouseDragging()) {
            clipEndDrag = true;
            originalClipStartTime = selectedClip->startTime;
        }
    }

    if(clipStartDrag) {
        if(mousePosSeconds > selectedClip->startTime) {
            selectedClip->offset = mousePosSeconds - selectedClip->startTime;
    		selectedClip->startTime = originalClipStartTime + selectedClip->offset;
        }
    }
}

bool TimelineComponent::handleEvents() {
    bool forceUpdate = app->isPlaying();

    if (this->isVisible() && !wasVisible) {
        syncSlidersToEngine();
        wasVisible = true;
    } else if (!this->isVisible()) {
        wasVisible = false;
    }

    if (muteMasterButton && muteMasterButton->isClicked() && app->getWindow().hasFocus()) {
        auto* masterTrack = app->getMasterTrack();
        masterTrack->toggleMute();
        muteMasterButton->m_modifier.setColor(
            masterTrack->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
        );

        muteMasterButton->setClicked(false);
        return true;
    }

    if (this->isVisible() && masterVolumeSlider) {
        const float newMasterVolDb = floatToDecibels(masterVolumeSlider->getValue());
        auto* masterTrack = app->getMasterTrack();
        constexpr float volumeTolerance = 0.001f;
        
        if (std::abs(masterTrack->getVolume() - newMasterVolDb) > volumeTolerance) {
            masterTrack->setVolume(newMasterVolDb);
            forceUpdate = true;
        }
    }

    const auto& allTracks = app->getAllTracks();
    std::set<std::string> engineTrackNames, uiTrackNames;
    
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name != "Master") {
            engineTrackNames.emplace(name);
        }
    }
    
    for (const auto& [name, _] : trackMuteButtons) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : trackVolumeSliders) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : trackSoloButtons) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : trackRemoveButtons) {
        uiTrackNames.emplace(name);
    }
    
    if (engineTrackNames != uiTrackNames) {
        if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
            timelineIt->second->clear();
        }
        trackMuteButtons.clear();
        trackVolumeSliders.clear();
        trackSoloButtons.clear();
        trackRemoveButtons.clear();
    }
    
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name == "Master") continue;
        
        const bool hasMuteButton = trackMuteButtons.count(name);
        const bool hasVolumeSlider = trackVolumeSliders.count(name);
        const bool hasSoloButton = trackSoloButtons.count(name);
        const bool hasRemoveButton = trackRemoveButtons.count(name);
        
        if (!hasMuteButton && !hasVolumeSlider && !hasSoloButton && !hasRemoveButton) {
            if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
                timelineIt->second->addElements({
                    spacer(Modifier().setfixedHeight(4.f)),
                    track(name, Align::TOP | Align::LEFT, t->getVolume(), t->getPan())
                });
                forceUpdate = true;
            }
        }
        
        if (auto muteBtnIt = trackMuteButtons.find(name); 
            muteBtnIt != trackMuteButtons.end() && muteBtnIt->second && muteBtnIt->second->isClicked() && app->getWindow().hasFocus()) {
            t->toggleMute();
            muteBtnIt->second->m_modifier.setColor(
                t->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
            );

            muteBtnIt->second->setClicked(false);
            forceUpdate = true;
        }
        
        if (auto soloBtnIt = trackSoloButtons.find(name); 
            soloBtnIt != trackSoloButtons.end() && soloBtnIt->second && soloBtnIt->second->isClicked() && app->getWindow().hasFocus()) {
            t->setSolo(!t->isSolo());
            soloBtnIt->second->m_modifier.setColor(
                t->isSolo() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
            );

            soloBtnIt->second->setClicked(false);
            forceUpdate = true;
        }
        
        if (this->isVisible()) { // Corrected isVisible() check
            if (auto sliderIt = trackVolumeSliders.find(name); 
                sliderIt != trackVolumeSliders.end() && sliderIt->second) {
                const float sliderDb = floatToDecibels(sliderIt->second->getValue());
                constexpr float volumeTolerance = 0.001f;
                if (std::abs(t->getVolume() - sliderDb) > volumeTolerance) {
                    t->setVolume(sliderDb);
                    forceUpdate = true;
                }
            }
        }
    }

    static bool prevCtrl = false, prevPlus = false, prevMinus = false, prevBackspace = false;
    const bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);

    // Block keyboard input when window doesn't have focus
    if (!app->getWindow().hasFocus()) {
        prevCtrl = ctrl;
        prevBackspace = backspace;
    } else {
        if (selectedClip && backspace && !prevBackspace) {
            for (const auto& t : allTracks) {
                if (t->getName() != app->getSelectedTrack()) continue;
                
                const auto& clips = t->getClips();
                for (size_t i = 0; i < clips.size(); ++i) {
                    const auto& clip = clips[i];
                    if (clip.startTime == selectedClip->startTime &&
                        clip.duration == selectedClip->duration &&
                        clip.sourceFile == selectedClip->sourceFile) {
                        t->removeClip(static_cast<int>(i));
                        selectedClip = nullptr;
                        forceUpdate = true;
                        DEBUG_PRINT("Removed selected clip from track '" << t->getName() << "'");
                        goto done_clip_remove;
                    }
                }
            }
            done_clip_remove: ;
        }

        constexpr float zoomSpeed = 0.2f;
        constexpr float maxZoom = 5.0f;
        constexpr float minZoom = 0.1f;
        
        float newZoom = app->uiState.timelineZoomLevel;

        const int vertScroll = app->ui->getVerticalScrollDelta();
        if (ctrl && vertScroll != 0) {
            constexpr float scrollZoomSpeed = 0.1f;
            const float zoomDelta = (vertScroll < 0) ? -scrollZoomSpeed : scrollZoomSpeed;
            newZoom = std::clamp(app->uiState.timelineZoomLevel + zoomDelta, minZoom, maxZoom);
        }
        
        if (newZoom != app->uiState.timelineZoomLevel) {
            const float currentOffset = timelineOffset;
            const float oldZoom = app->uiState.timelineZoomLevel;
            const sf::Vector2f mousePos = app->ui->getMousePosition();
            
            sf::Vector2f timelineRowPos(0.f, 0.f);
            if (!allTracks.empty()) {
                const std::string rowKey = allTracks[0]->getName() + "_scrollable_row";
                if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                    timelineRowPos = rowIt->second->getPosition();
                }
            }
            
            const float mouseXInTimeline = mousePos.x - timelineRowPos.x;
            app->uiState.timelineZoomLevel = newZoom;
            const float zoomRatio = newZoom / oldZoom;
            timelineOffset = mouseXInTimeline - (mouseXInTimeline - currentOffset) * zoomRatio;
            
            for (const auto& track : allTracks) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                    scrollableRow->setOffset(std::min(0.f, timelineOffset));
                }
            }
            forceUpdate = true;
        }

        prevCtrl = ctrl;
        prevBackspace = backspace;
    }

    app->ui->resetScrollDeltas();

    // Update track highlighting based on current selection
    const std::string selectedTrack = app->getSelectedTrack();
    for (const auto& t : allTracks) {
        if (t->getName() == "Master") continue;
        
        const std::string labelKey = t->getName() + "_label";
        if (auto labelIt = containers.find(labelKey); labelIt != containers.end() && labelIt->second) {
            auto* labelColumn = labelIt->second;
            if (t->getName() == selectedTrack) {
                labelColumn->m_modifier.setColor(app->resources.activeTheme->selected_track_color);
            } else {
                // Track is not selected - use normal color
                labelColumn->m_modifier.setColor(app->resources.activeTheme->track_color);
            }
        }
    }

    // Handle Master track highlighting
    if (selectedTrack == "Master") {
        const std::string masterLabelKey = "Master_Track_Column";
        if (auto masterIt = containers.find(masterLabelKey); masterIt != containers.end() && masterIt->second) {
            auto* masterColumn = masterIt->second;
            masterColumn->m_modifier.setColor(app->resources.activeTheme->selected_track_color);
        }
    } else {
        const std::string masterLabelKey = "Master_Track_Column";
        if (auto masterIt = containers.find(masterLabelKey); masterIt != containers.end() && masterIt->second) {
            auto* masterColumn = masterIt->second;
            masterColumn->m_modifier.setColor(app->resources.activeTheme->master_track_color);
        }
    }

    if (app->freshRebuild) rebuildUI();
    return forceUpdate;
}

uilo::Row* TimelineComponent::masterTrack() {
    muteMasterButton = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "mute",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_Master"
    );

    masterVolumeSlider = slider(
        Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        "Master_volume_slider"
    );

    auto* masterTrackColumn = column(
        Modifier()
            .align(Align::RIGHT)
            .setfixedWidth(196)
            .setColor(app->resources.activeTheme->master_track_color),
    contains{
        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        row(
            Modifier(),
        contains{
            spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

            column(
                Modifier(),
            contains{
                text(
                    Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                    "Master",
                    app->resources.dejavuSansFont
                ),

                row(
                    Modifier(),
                contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                    muteMasterButton
                }),
            }),

            masterVolumeSlider,

            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),

        }, "Master_Track_Label"),

        spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
    }, "Master_Track_Column");

    // Register the master track column for dynamic highlighting
    containers["Master_Track_Column"] = masterTrackColumn;

    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(96)
            .align(Align::LEFT | Align::BOTTOM)
            .onLClick([&](){
                if (!app->getWindow().hasFocus()) return;
                app->setSelectedTrack("Master");
            })
            .onRClick([&](){
                if (!app->getWindow().hasFocus()) return;
                app->setSelectedTrack("Master");
            }),
    contains{
        masterTrackColumn
    }, "Master_Track");
}

uilo::Row* TimelineComponent::track(
    const std::string& trackName, 
    Align alignment, 
    float volume,
    float pan
) {
    trackMuteButtons[trackName] = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(32).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "M",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_" + trackName
    );

    trackSoloButtons[trackName] = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(32).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "S",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "solo_" + trackName
    );

    trackRemoveButtons[trackName] = button(
        Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedWidth(16).setfixedHeight(16).setColor(app->resources.activeTheme->mute_color)
            .onLClick([this, trackName](){
                if (!app->getWindow().hasFocus()) return;
                app->removeTrack(trackName);
            }),
        ButtonStyle::Pill,
        "",
        "",
        sf::Color::Transparent,
        "remove_" + trackName
    );

    trackVolumeSliders[trackName] = slider(
        Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        trackName + "_volume_slider"
    );

    auto* scrollableRowElement = scrollableRow(
        Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains {}, trackName + "_scrollable_row"
    );
    containers[trackName + "_scrollable_row"] = scrollableRowElement;

    // Use a single lambda with a flag to handle both left and right clicks
    auto handleTrackClick = [this, trackName](bool isRightClick) {
        if (!app->getWindow().hasFocus()) return;
        
        if (!(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl))) {
            app->setSelectedTrack(trackName);
            return;
        }

        auto track = app->getTrack(trackName);
        if (!track) return;

        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        auto* trackRow = containers[trackName + "_scrollable_row"];
        if (!trackRow) return;
        sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();

        float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineOffset, timelineOffset);
        
        if (isRightClick) {
            const auto& clips = track->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                if (timePosition >= clips[i].startTime && timePosition <= (clips[i].startTime + clips[i].duration)) {
                    DEBUG_PRINT("Removed clip from track '" << track->getName() << "' at time: " << clips[i].startTime << " seconds");
                    track->removeClip(i);
                    break;
                }
            }
        } else { // Left click (with Ctrl)
            if (track->getReferenceClip()) {
                auto lines = generateTimelineMeasures(100.f * app->uiState.timelineZoomLevel, timelineOffset, trackRow->getSize(), 4, 4, &app->resources);
                float snapX = getNearestMeasureX(localMousePos, lines);
                timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, snapX - timelineOffset, timelineOffset);

                AudioClip* newClip = track->getReferenceClip();
                track->addClip(AudioClip(newClip->sourceFile, timePosition, 0.0, newClip->duration, 1.0f));
                DEBUG_PRINT("Added clip to track '" << track->getName() << "' at time: " << timePosition << " seconds");
            }
        }
    };

    scrollableRowElement->m_modifier.onLClick([=]() { handleTrackClick(false); });
    scrollableRowElement->m_modifier.onRClick([=]() { handleTrackClick(true); });

    auto* trackLabelColumn = column(
        Modifier()
            .align(Align::RIGHT)
            .setfixedWidth(196)
            .setColor(app->resources.activeTheme->track_color),
    contains{
        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        row(
            Modifier().align(Align::RIGHT).setHighPriority(true),
        contains{
            column(
                Modifier().setfixedWidth(32).align(Align::LEFT | Align::TOP), 
            contains{
                    trackRemoveButtons[trackName],
            }),

            column(
                Modifier(),
            contains{
                row(
                    Modifier().align(Align::LEFT | Align::TOP),
                contains{
                    spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
                    
                    text(
                        Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        trackName,
                        app->resources.dejavuSansFont
                    )
                }),

                row(
                    Modifier(),
                contains{
                    spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

                    trackMuteButtons[trackName],
                    
                    spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
                    
                    trackSoloButtons[trackName]
                }),
            }),

            trackVolumeSliders[trackName],

            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),

        spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
    }, trackName + "_label");

    // Register the label container for dynamic highlighting
    containers[trackName + "_label"] = trackLabelColumn;

    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(96)
            .align(alignment)
            .onLClick([&](){
                if (!app->getWindow().hasFocus()) return;
                app->setSelectedTrack(trackName);
            })
            .onRClick([&](){
                if (!app->getWindow().hasFocus()) return;
                app->setSelectedTrack(trackName);
            }),
    contains{
        scrollableRowElement,
        trackLabelColumn
    }, trackName + "_track_row");
}

void TimelineComponent::handleCustomUIElements() {
    static bool prevCtrlPressed = false;
    static bool prevBackspace = false;
    static std::string prevSelectedTrack = "";
    
    const bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
    
    // Get current selected track from Engine
    const std::string currentSelectedTrack = app->getSelectedTrack();
    if (currentSelectedTrack != prevSelectedTrack) {
        selectedClip = nullptr;
        prevSelectedTrack = currentSelectedTrack;
    }
    
    auto timelineIt = containers.find("timeline");
    if (timelineIt == containers.end() || !timelineIt->second) return;
    auto* timelineElement = static_cast<ScrollableColumn*>(timelineIt->second);

    const auto& allTracks = app->getAllTracks();
    if (allTracks.empty()) return;

    float newMasterOffset = timelineOffset;

    const double bpm = app->getBpm();
    const float zoomLevel = app->uiState.timelineZoomLevel;
    const float beatWidth = 100.f * zoomLevel;
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const bool isPlaying = app->isPlaying();
    const sf::Vector2f mousePos = app->ui->getMousePosition();

    if (selectedClip && backspace && !prevBackspace && app->getWindow().hasFocus()) {
        for (auto& t : allTracks) {
            if (t->getName() != currentSelectedTrack) continue;
            
            const auto& clips = t->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                const auto& clip = clips[i];
                if (clip.startTime == selectedClip->startTime &&
                    clip.duration == selectedClip->duration &&
                    clip.sourceFile == selectedClip->sourceFile) {
                    t->removeClip(static_cast<int>(i));
                    selectedClip = nullptr;
                    DEBUG_PRINT("Removed selected clip from track '" << t->getName() << "'");
                    goto done_clip_remove;
                }
            }
        }
        done_clip_remove: ;
    }

    // Find offset from any scrollable row
    for (const auto& track : allTracks) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        auto rowIt = containers.find(rowKey);
        if (rowIt == containers.end()) continue;
        
        auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
        if (scrollableRow->getOffset() != timelineOffset) {
            newMasterOffset = scrollableRow->getOffset();
            break;
        }
    }

    // Auto-follow playhead during playback
    if (isPlaying) {
        const float playheadXPos = secondsToXPosition(bpm, beatWidth, std::round(app->getPosition() * 1000.0) / 1000.0);
        float visibleWidth = 0.f;
        
        for (const auto& track : allTracks) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            auto rowIt = containers.find(rowKey);
            if (rowIt != containers.end() && rowIt->second) {
                visibleWidth = rowIt->second->getSize().x;
                break;
            }
        }
        
        if (visibleWidth > 0.f) {
            const float centerPos = visibleWidth * 0.5f;
            const float targetOffset = -(playheadXPos - centerPos);
            // Frame-rate independent follow speed (pixels per second)
            constexpr float followSpeedPerSecond = 800.0f;
            const float followSpeed = followSpeedPerSecond * deltaTime;
            const float offsetDelta = (targetOffset - newMasterOffset) * std::min(followSpeed, 1.0f);
            newMasterOffset = newMasterOffset + offsetDelta;
        }
    }

    const float clampedOffset = std::min(0.f, newMasterOffset);

    const bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    constexpr float baseScrollSpeedPerSecond = 1800.0f;
    const float frameRateIndependentScrollSpeed = ctrlHeld ? 0.0f : baseScrollSpeedPerSecond * deltaTime;

    timelineElement->setScrollSpeed(frameRateIndependentScrollSpeed);
    
    for (const auto& track : allTracks) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end()) {
            auto* trackRow = rowIt->second;
            auto* scrollableRow = static_cast<ScrollableRow*>(trackRow);

            scrollableRow->setScrollSpeed(frameRateIndependentScrollSpeed);
            scrollableRow->setOffset(clampedOffset);

            const sf::Vector2f trackRowPos = trackRow->getPosition();
            const sf::Vector2f trackRowSize = trackRow->getSize();
            const float timelineBottom = timelineElement->getPosition().y + timelineElement->getSize().y;
            const float timelineTop = timelineElement->getPosition().y;
            
            if (trackRowPos.y + trackRowSize.y < timelineTop || trackRowPos.y > timelineBottom)
                continue;

            auto lines = generateTimelineMeasures(beatWidth, clampedOffset, trackRow->getSize(), 4, 4, &app->resources);
            const auto& clipsVec = track->getClips();
            const sf::Vector2f localMousePos = mousePos - trackRowPos;
            
            for (size_t i = 0; i < clipsVec.size(); ++i) {
                const auto& ac = clipsVec[i];
                const float clipWidthPixels = ac.duration * pixelsPerSecond;
                const float clipXPosition = (ac.startTime * pixelsPerSecond) + clampedOffset;
                const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
                
                if (clipRect.contains(localMousePos)) {
                    if (!app->getWindow().hasFocus()) continue;
                    if (!ctrlPressed && !prevCtrlPressed && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                        selectedClip = const_cast<AudioClip*>(&clipsVec[i]);
                        app->setSelectedTrack(track->getName());
                        if (!isPlaying)
                            app->setSavedPosition(ac.startTime);
                    }
                }
            }

            auto clips = generateClipRects(bpm, beatWidth, clampedOffset, trackRow->getSize(), 
                                         track->getClips(), 0.f, &app->resources, &app->uiState, selectedClip, 
                                         track->getName(), currentSelectedTrack);

            std::vector<std::shared_ptr<sf::Drawable>> rowGeometry;
            rowGeometry.reserve(clips.size() + lines.size());
            
            rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(clips.begin()), std::make_move_iterator(clips.end()));
            rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
            scrollableRow->setCustomGeometry(rowGeometry);
        }
    }
    prevCtrlPressed = ctrlPressed;
    prevBackspace = backspace;

    if (app->getAllTracks().size() > 1) {
        auto playhead = getPlayHead(
            app->getBpm(), 
            100.f * app->uiState.timelineZoomLevel,
            clampedOffset,
            app->getPosition(),
            timelineElement->getSize()
        );

        std::vector<std::shared_ptr<sf::Drawable>> timelineGeometry;
        timelineGeometry.push_back(playhead);
        timelineElement->setCustomGeometry(timelineGeometry);
        timelineOffset = clampedOffset;
    }

    if (selectedClip && !app->isPlaying() && app->getPosition() != selectedClip->startTime) {
        if (!currentSelectedTrack.empty()) {
            app->setSavedPosition(selectedClip->startTime);
        }
    }
}

void TimelineComponent::rebuildUI() {
    auto* baseTimelineColumn = getColumn("base_timeline_column");
    if (!baseTimelineColumn) return;

    baseTimelineColumn->clear();

    masterTrackElement = masterTrack();
    
    auto* timelineScrollable = scrollableColumn(
        Modifier(),
        contains{}, "timeline"
    );
    containers["timeline"] = timelineScrollable;
    
    const auto& allTracks = app->getAllTracks();
    containers.reserve(containers.size() + allTracks.size());
    
    for (const auto& t : allTracks) {
        if (t->getName() == "Master") continue;
        
        auto* trackRowElem = track(t->getName(), Align::TOP | Align::LEFT, t->getVolume(), t->getPan());
        timelineScrollable->addElement(spacer(Modifier().setfixedHeight(4.f)));
        timelineScrollable->addElement(trackRowElem);
        
        if (trackRowElem) {
            const auto& elements = trackRowElem->getElements();
            if (!elements.empty() && elements[0]) {
                const std::string rowKey = t->getName() + "_scrollable_row";
                containers.emplace(rowKey, static_cast<uilo::Container*>(elements[0]));
            }
        }
    }

    baseTimelineColumn->addElements({
        timelineScrollable,
        masterTrackElement
    });
}

void TimelineComponent::rebuildUIFromEngine() {}

void TimelineComponent::syncSlidersToEngine() {
    if (app->getMasterTrack() && masterVolumeSlider) {
        float engineVol = app->getMasterTrack()->getVolume();
        float sliderValue = decibelsToFloat(engineVol);
        masterVolumeSlider->setValue(sliderValue);
    }
    
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        if (auto volumeSlider = trackVolumeSliders.find(track->getName());
            volumeSlider != trackVolumeSliders.end() && volumeSlider->second) {
            float engineVol = track->getVolume();
            float sliderValue = decibelsToFloat(engineVol);
            volumeSlider->second->setValue(sliderValue);
        }
    }
}

inline std::unordered_map<std::string, std::vector<float>>& getWaveformCache() {
    static std::unordered_map<std::string, std::vector<float>> s_waveformCache;
    return s_waveformCache;
}

inline void clearWaveformCache() {
    auto& cache = getWaveformCache();
    DEBUG_PRINT("DEBUG: Clearing waveform cache (" << cache.size() << " entries)");
    cache.clear();
}

inline void ensureWaveformIsCached(const AudioClip& clip) {
    if (!clip.sourceFile.existsAsFile()) return;

    auto& cache = getWaveformCache();
    const std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    
    if (cache.find(filePath) != cache.end()) return;

    static thread_local juce::AudioFormatManager formatManager;
    static thread_local bool initialized = false;
    if (!initialized) {
        formatManager.registerBasicFormats();
        initialized = true;
    }
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(clip.sourceFile));
    if (!reader) {
        cache.emplace(filePath, std::vector<float>{});
        return;
    }

    const long long totalSamples = reader->lengthInSamples;
    if (totalSamples == 0) {
        cache.emplace(filePath, std::vector<float>{});
        return;
    }
    
    constexpr float peakResolution = 0.05f;
    const int desiredPeaks = std::max(1, static_cast<int>(std::ceil(clip.duration / peakResolution)));
    const long long samplesPerPeak = std::max(1LL, totalSamples / desiredPeaks);

    std::vector<float> peaks;
    peaks.reserve(desiredPeaks);

    const int bufferSize = static_cast<int>(std::min(samplesPerPeak, 8192LL));
    juce::AudioBuffer<float> buffer(reader->numChannels, bufferSize);

    for (int i = 0; i < desiredPeaks; ++i) {
        const long long startSample = static_cast<long long>(i) * samplesPerPeak;
        if (startSample >= totalSamples) break;

        const int numSamplesToRead = static_cast<int>(
            std::min(static_cast<long long>(bufferSize), 
                    std::min(samplesPerPeak, totalSamples - startSample))
        );
        
        reader->read(&buffer, 0, numSamplesToRead, startSample, true, true);

        float maxAmplitude = 0.0f;
        for (int channel = 0; channel < reader->numChannels; ++channel) {
            const float channelMagnitude = buffer.getMagnitude(channel, 0, numSamplesToRead);
            maxAmplitude = std::max(maxAmplitude, channelMagnitude);
        }
        peaks.emplace_back(maxAmplitude);
    }

    cache.emplace(filePath, std::move(peaks));
}

inline std::vector<std::shared_ptr<sf::Drawable>> generateTimelineMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator,
    unsigned int sigDenominator,
    UIResources* resources
) {
    if (measureWidth <= 0.f || sigNumerator == 0 || !resources) return {};
    
    std::vector<std::shared_ptr<sf::Drawable>> lines;
    
    constexpr float margin = 10.f;
    const float visibleWidth = rowSize.x;
    const float startX = -scrollOffset;
    const float endX = startX + visibleWidth;
    
    const int startMeasure = static_cast<int>(std::floor(startX / measureWidth));
    const int endMeasure = static_cast<int>(std::ceil(endX / measureWidth)) + 1;
    
    const float beatWidth = measureWidth / sigNumerator;
    const sf::Color& lineColor = resources->activeTheme->line_color;
    sf::Color transparentLineColor = lineColor;
    transparentLineColor.a = 100;
    
    const int measureCount = endMeasure - startMeasure + 1;
    const int totalLines = measureCount * sigNumerator;
    lines.reserve(totalLines);
    
    for (int measure = startMeasure; measure <= endMeasure; ++measure) {
        const float xPos = static_cast<float>(measure) * measureWidth + scrollOffset;
        
        if (xPos >= -margin && xPos <= visibleWidth + margin) {
            auto measureLine = std::make_shared<sf::RectangleShape>();
            measureLine->setSize({2.f, rowSize.y});
            measureLine->setPosition({xPos, 0.f});
            measureLine->setFillColor(lineColor);
            lines.emplace_back(std::move(measureLine));
        }
        
        for (unsigned int beat = 1; beat < sigNumerator; ++beat) {
            const float beatX = static_cast<float>(beat) * beatWidth + xPos;
            
            if (beatX >= -margin && beatX <= visibleWidth + margin) {
                auto subMeasureLine = std::make_shared<sf::RectangleShape>();
                subMeasureLine->setSize({1.f, rowSize.y});
                subMeasureLine->setPosition({beatX, 0.f});
                subMeasureLine->setFillColor(transparentLineColor);
                lines.emplace_back(std::move(subMeasureLine));
            }
        }
    }
    
    return lines;
}

inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(
    double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, 
    const std::vector<AudioClip>& clips, float verticalOffset, 
    UIResources* resources, UIState* uiState, const AudioClip* selectedClip,
    const std::string& currentTrackName, const std::string& selectedTrackName
) {
    if (clips.empty()) return {};
    
    std::vector<std::shared_ptr<sf::Drawable>> clipRects;
    clipRects.reserve(clips.size() * 2);
    
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const sf::Color& clipColor = resources->activeTheme->clip_color;

    for (const auto& ac : clips) {
        const float clipWidthPixels = ac.duration * pixelsPerSecond;
        const float clipXPosition = (ac.startTime * pixelsPerSecond) + scrollOffset;

        const bool isSelected = selectedClip &&
                                currentTrackName == selectedTrackName &&
                                ac.startTime == selectedClip->startTime &&
                                ac.duration == selectedClip->duration &&
                                ac.sourceFile == selectedClip->sourceFile;

        if (isSelected) {
            auto outlineRect = std::make_shared<sf::RectangleShape>();
            outlineRect->setSize({clipWidthPixels, rowSize.y});
            outlineRect->setPosition({clipXPosition, 0.f});
            outlineRect->setFillColor(sf::Color(
                255 - clipColor.r,
                255 - clipColor.g,
                255 - clipColor.b
            ));
            clipRects.emplace_back(std::move(outlineRect));

            auto clipRect = std::make_shared<sf::RectangleShape>();
            const float insetThickness = 3.f;
            clipRect->setSize({clipWidthPixels - 2 * insetThickness, rowSize.y - 2 * insetThickness});
            clipRect->setPosition({clipXPosition + insetThickness, insetThickness});
            clipRect->setFillColor(clipColor);
            clipRects.emplace_back(std::move(clipRect));
        } else {
            auto clipRect = std::make_shared<sf::RectangleShape>();
            clipRect->setSize({clipWidthPixels, rowSize.y});
            clipRect->setPosition({clipXPosition, 0.f});
            clipRect->setFillColor(clipColor);
            clipRects.emplace_back(std::move(clipRect));
        }

        auto waveformDrawables = generateWaveformData(
            ac,
            sf::Vector2f(clipXPosition, 0.f),
            sf::Vector2f(clipWidthPixels, rowSize.y),
            verticalOffset,
            resources,
            uiState
        );
        
        clipRects.insert(clipRects.end(),
                        std::make_move_iterator(waveformDrawables.begin()),
                        std::make_move_iterator(waveformDrawables.end()));
    }
    
    return clipRects;
}

inline std::shared_ptr<sf::Drawable> getPlayHead(double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize) {
    auto playHeadRect = std::make_shared<sf::RectangleShape>();

    const float xPosition = secondsToXPosition(bpm, beatWidth, seconds);
    
    constexpr float playheadWidth = 4.f;
    constexpr sf::Color playheadColor(255, 0, 0, 100);

    playHeadRect->setSize({playheadWidth, rowSize.y});
    playHeadRect->setPosition({xPosition + scrollOffset, 0.f});
    playHeadRect->setFillColor(playheadColor);
    
    return playHeadRect;
}

inline float getNearestMeasureX(const sf::Vector2f& pos, const std::vector<std::shared_ptr<sf::Drawable>>& lines) {
    if (lines.empty()) return pos.x;
    
    float closestX = 0.0f;
    float minDistance = std::numeric_limits<float>::max();
    
    for (const auto& line : lines) {
        if (const auto rect = std::dynamic_pointer_cast<const sf::RectangleShape>(line)) {
            const float lineX = rect->getPosition().x;
            const float distance = std::abs(pos.x - lineX);
            
            if (distance < minDistance) {
                minDistance = distance;
                closestX = lineX;
            }
        }
    }
    
    return closestX;
}

inline float secondsToXPosition(double bpm, float beatWidth, float seconds) noexcept {
    constexpr float secondsPerMinute = 60.0f;
    const float pixelsPerSecond = (beatWidth * static_cast<float>(bpm)) / secondsPerMinute;
    return seconds * pixelsPerSecond;
}

inline float xPosToSeconds(double bpm, float beatWidth, float xPos, float scrollOffset) noexcept {
    constexpr float secondsPerMinute = 60.0f;
    const float pixelsPerSecond = (beatWidth * static_cast<float>(bpm)) / secondsPerMinute;
    return xPos / pixelsPerSecond;
}

inline std::vector<std::shared_ptr<sf::Drawable>> generateWaveformData(
    const AudioClip& clip, const sf::Vector2f& clipPosition,
    const sf::Vector2f& clipSize, float verticalOffset,
    UIResources* resources, UIState* uiState
) {
    ensureWaveformIsCached(clip);

    const auto& cache = getWaveformCache();
    const std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    
    auto cacheIt = cache.find(filePath);
    if (cacheIt == cache.end()) return {};
    
    const auto& peaks = cacheIt->second;
    if (peaks.empty() || clipSize.x <= 0) return {};

    constexpr int upsample = 5;
    constexpr float waveformScale = 0.9f;
    constexpr float peakThreshold = 0.001f;
    
    const int numPeaks = static_cast<int>(peaks.size());
    const int numSamples = numPeaks * upsample;
    
    sf::Color waveformColorWithAlpha = resources->activeTheme->wave_form_color;
    waveformColorWithAlpha.a = 180;

    const float invNumSamples = 1.0f / numSamples;
    const float lineHeightScale = clipSize.y * waveformScale;
    const float baseLineY = clipPosition.y + clipSize.y * 0.5f + verticalOffset;

    auto vertexArray = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
    vertexArray->resize(numSamples * 2);

    size_t vertexIndex = 0;
    for (int i = 0; i < numSamples; ++i) {
        const float t = i * invNumSamples * (numPeaks - 1);
        const int idx = static_cast<int>(t);
        const float frac = t - idx;
        
        float peakValue = peaks[idx];
        if (idx + 1 < numPeaks) {
            peakValue = std::fma(peaks[idx + 1] - peaks[idx], frac, peaks[idx]);
        }
        
        if (peakValue > peakThreshold) {
            const float lineHeight = peakValue * lineHeightScale;
            const float lineX = std::fma(i * invNumSamples, clipSize.x, clipPosition.x);
            const float lineYTop = baseLineY - lineHeight * 0.5f;
            const float lineYBottom = baseLineY + lineHeight * 0.5f;
            
            if (vertexIndex + 1 < vertexArray->getVertexCount()) {
                (*vertexArray)[vertexIndex].position = sf::Vector2f(lineX, lineYTop);
                (*vertexArray)[vertexIndex].color = waveformColorWithAlpha;
                (*vertexArray)[vertexIndex + 1].position = sf::Vector2f(lineX, lineYBottom);
                (*vertexArray)[vertexIndex + 1].color = waveformColorWithAlpha;
                vertexIndex += 2;
            }
        }
    }
    
    vertexArray->resize(vertexIndex);
    
    std::vector<std::shared_ptr<sf::Drawable>> result;
    if (vertexArray->getVertexCount() > 0) {
        result.emplace_back(std::move(vertexArray));
    }
    
    return result;
}

// Plugin interface for TimelineComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(TimelineComponent)