#pragma once

#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"
#include "../../src/audio/MIDIClip.hpp"
#include "../../src/audio/MIDITrack.hpp"

#include <chrono>
#include <cmath>
#include <set>
#include <unordered_map>
#include <filesystem>

class TimelineComponent : public MULOComponent {
public:
    static TimelineComponent* instance;
    
    AudioClip* selectedClip = nullptr;
    
    struct SelectedMIDIClipInfo {
        bool hasSelection = false;
        double startTime = 0.0;
        double duration = 0.0;
        std::string trackName = "";
    } selectedMIDIClipInfo;

    TimelineComponent();
    ~TimelineComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;
    inline Container* getLayout() override { return layout; }

    void rebuildUI();

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

private:
    float lastScrubberPosition = 0.0f;
    bool scrubberPositionChanged = false;
    float expectedTimelineOffset = 0.0f;

    struct TimelineState {
        float timelineOffset = 0.f;
        bool wasVisible = true;
        float deltaTime = 0.0f;
        bool firstFrame = true;
        std::chrono::steady_clock::time_point lastFrameTime;
        double virtualCursorTime = 0.0;
        bool showVirtualCursor = false;
        std::chrono::steady_clock::time_point lastBlinkTime;
        bool virtualCursorVisible = true;
    } timelineState;

    struct DragState {
        bool isDraggingClip = false;
        bool isDraggingAudioClip = false;
        bool isDraggingMIDIClip = false;
        bool clipSelectedForDrag = false;
        AudioClip* draggedAudioClip = nullptr;
        MIDIClip* draggedMIDIClip = nullptr;
        sf::Vector2f dragStartMousePos;
        double dragStartClipTime = 0.0;
        double dragMouseOffsetInClip = 0.0;
        sf::Vector2f draggedTrackRowPos;
        std::string draggedTrackName;
        
        static constexpr float DRAG_THRESHOLD = 10.0f;
    } dragState;

    struct PlacementState {
        bool isDraggingPlacement = false;
        bool isDraggingDeletion = false;
        std::string currentSelectedTrack;
        std::set<double> processedPositions;
    } placementState;

    struct CacheState {
        std::vector<std::shared_ptr<sf::Drawable>> cachedMeasureLines;
        float lastMeasureWidth = -1.f;
        float lastScrollOffset = -1.f;
        sf::Vector2f lastRowSize = {-1.f, -1.f};
    } cacheState;

    struct ClipboardState {
        std::vector<AudioClip> copiedAudioClips;
        std::vector<MIDIClip> copiedMIDIClips;
        bool hasClipboard = false;
    } clipboardState;

    struct UIElements {
        Row* masterTrackElement = nullptr;
        Button* muteMasterButton = nullptr;
        Slider* masterVolumeSlider = nullptr;
        Row* masterTrackLabel = nullptr;
        std::unordered_map<std::string, Button*> trackMuteButtons;
        std::unordered_map<std::string, Slider*> trackVolumeSliders;
        std::unordered_map<std::string, Button*> trackSoloButtons;
        std::unordered_map<std::string, Button*> trackRemoveButtons;
    } uiElements;

    void handleAllUserInput();
    void handleMouseInput();
    void handleKeyboardInput();
    void handleZoomChange(float newZoom);
    void handleClipInteractions();
    void handleTrackLeftClick();
    void handleTrackRightClick();
    
    void processDragOperations();
    void handleClipDragOperations();
    void handlePlacementDragOperations();
    void handleDeletionDragOperations();
    void updateDragState();
    
    void updateTimelineVisuals();
    void updateScrolling();
    void updatePlayheadFollowing();
    void renderTrackContent();
    void updateVirtualCursor();
    
    void syncUIToEngine();
    void handleMasterTrackControls();
    void handleTrackControls();
    void handleIndividualTrackControls(Track* track, const std::string& name);
    void updateTrackHighlighting();
    void rebuildTrackUI();
    
    void updateTimelineState();
    void resetDragState();
    void resetPlacementState();
    void clearProcessedPositions();

    Row* masterTrack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);

    void handleCustomUIElements();
    void handleDragOperations();
    void handleClipSelection();
    void handleTrackInteractions();
    void updateVisualState();
    
    void rebuildUIFromEngine();
    void syncSlidersToEngine();
    void processClipAtPosition(Track* track, const sf::Vector2f& localMousePos, bool isRightClick);
    
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedMeasureLines(float measureWidth, float scrollOffset, const sf::Vector2f& rowSize);
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedClipGeometry(const std::string& trackName, double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, UIResources* resources, UIState* uiState, const AudioClip* selectedClip, const std::string& currentTrackName, const std::string& selectedTrackName);
    
    // Clipboard functions
    void copySelectedClips();
    void pasteClips();
    void duplicateSelectedClips();
    void clearClipboard();
    
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
    timelineState.lastFrameTime = std::chrono::steady_clock::now();
    timelineState.lastBlinkTime = std::chrono::steady_clock::now();
    timelineState.deltaTime = 0.0f;
    timelineState.firstFrame = true;
    
    // Set the static instance pointer for global access
    instance = this;
}

TimelineComponent::~TimelineComponent() {
    selectedClip = nullptr;
    // Clear the static instance pointer
    if (instance == this) {
        instance = nullptr;
    }
    app->writeConfig("scrubber_position", 0.f);
}

void TimelineComponent::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;

    relativeTo = "file_browser";
    uiElements.masterTrackElement = masterTrack();

    // Test filesystem access for trusted plugin
    std::filesystem::create_directories("/tmp/muloui");
    std::ofstream("/tmp/muloui/testfile.txt") << "TimelineComponent test file" << std::endl;

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
            uiElements.masterTrackElement
        }, "base_timeline_column")
    });

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void TimelineComponent::update() {
    if (!this->isVisible()) return;
    
    updateTimelineState();
    
    // Check if scrubber is being dragged to disable timeline mouse input
    bool scrubberDragging = app->readConfig<bool>("scrubber_dragging", false);
    features.enableMouseInput = !scrubberDragging;

    if (features.enableMouseInput || features.enableKeyboardInput) {
        handleAllUserInput();
    }
    
    updateTimelineVisuals();
    
    handleCustomUIElements();

    // Check if scrubber position has changed
    float scrubberPos = app->readConfig<float>("scrubber_position", 0.0f);
    scrubberPositionChanged = (std::abs(scrubberPos - lastScrubberPosition) > 0.001f);
    
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
    float currentTimelineOffset = timelineState.timelineOffset;
    
    for (const auto& track : app->getAllTracks()) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
            float actualOffset = scrollableRow->getOffset();
            float diff = std::abs(actualOffset - expectedTimelineOffset);
            
            if (diff > 0.01f) {
                currentTimelineOffset = actualOffset;
                timelineState.timelineOffset = actualOffset;
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
        
        timelineState.timelineOffset = -scrubberPixelPos;
        expectedTimelineOffset = timelineState.timelineOffset;

        for (const auto& track : app->getAllTracks()) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                scrollableRow->setOffset(std::min(0.f, timelineState.timelineOffset));
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
        
        timelineState.timelineOffset = currentTimelineOffset;
        expectedTimelineOffset = timelineState.timelineOffset;
        
        for (const auto& track : app->getAllTracks()) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                scrollableRow->setOffset(timelineState.timelineOffset);
            }
        }
    }

    // Record timeline width for scrubber bar size calculation
    float timelineViewWidth = layout->getSize().x - uiElements.masterTrackLabel->getSize().x;
    float timelineStart = secondsToXPosition(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, 0.0);
    float timelineEnd = secondsToXPosition(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, lastClipEndSeconds);
    float totalTimelineWidth = timelineEnd - timelineStart;
    app->writeConfig<float>("scrubber_width_ratio", timelineViewWidth / totalTimelineWidth);

    // Where the scrubber bar should start (percentage)
    if (-timelineState.timelineOffset <= totalTimelineWidth) {
        float viewStartRatio = -timelineState.timelineOffset / totalTimelineWidth;
        app->writeConfig("scrubber_start_ratio", viewStartRatio);
    }

}

void TimelineComponent::updateTimelineState() {
    auto currentTime = std::chrono::steady_clock::now();
    
    if (!timelineState.firstFrame) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - timelineState.lastFrameTime);
        timelineState.deltaTime = duration.count() / 1000000.0f;
        
        constexpr float maxDeltaTime = 1.0f / 30.0f;
        timelineState.deltaTime = std::min(timelineState.deltaTime, maxDeltaTime);
    } else {
        timelineState.deltaTime = 1.0f / 60.0f;
        timelineState.firstFrame = false;
    }
    
    timelineState.lastFrameTime = currentTime;
}

bool TimelineComponent::handleEvents() {
    bool forceUpdate = app->isPlaying();

    if (this->isVisible() && !timelineState.wasVisible) {
        syncSlidersToEngine();
        timelineState.wasVisible = true;
    } else if (!this->isVisible()) {
        timelineState.wasVisible = false;
    }

    if (features.enableUISync) {
        syncUIToEngine();
    }

    if (app->freshRebuild) rebuildUI();
    
    return forceUpdate;
}

uilo::Row* TimelineComponent::masterTrack() {
    uiElements.muteMasterButton = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "mute",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_Master"
    );

    uiElements.masterVolumeSlider = slider(
        Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Vertical,
        "Master_volume_slider"
    );

    uiElements.masterTrackLabel = row(
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

                uiElements.muteMasterButton
            }),
        }),

        uiElements.masterVolumeSlider,

        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),

    }, "Master_Track_Label");

    auto* masterTrackColumn = column(
        Modifier()
            .align(Align::RIGHT)
            .setfixedWidth(196)
            .setColor(app->resources.activeTheme->master_track_color),
    contains{
        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        uiElements.masterTrackLabel,

        spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
    }, "Master_Track_Column");

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
    uiElements.trackMuteButtons[trackName] = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(32).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "M",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_" + trackName
    );

    uiElements.trackSoloButtons[trackName] = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(32).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "S",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "solo_" + trackName
    );

    uiElements.trackRemoveButtons[trackName] = button(
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

    uiElements.trackVolumeSliders[trackName] = slider(
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

    auto handleTrackLeftClick = [this, trackName]() {
        if (!app->getWindow().hasFocus()) return;
        
        auto track = app->getTrack(trackName);
        if (!track) return;

        app->setSelectedTrack(trackName);
        placementState.currentSelectedTrack = trackName;
        
        bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        if (!ctrlPressed || !app->ui->isMouseDragging()) {
            sf::Vector2f globalMousePos = app->ui->getMousePosition();
            auto* trackRow = containers[trackName + "_scrollable_row"];
            if (trackRow) {
                sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();
                float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineState.timelineOffset, timelineState.timelineOffset);
                timelineState.virtualCursorTime = timePosition;
                timelineState.showVirtualCursor = true;
                if (!app->isPlaying()) {
                    app->setPosition(timelineState.virtualCursorTime);
                }
                app->setSavedPosition(timelineState.virtualCursorTime);
            }
        }
        
        placementState.processedPositions.clear();
    };
    
    auto handleTrackRightClick = [this, trackName]() {
        if (!app->getWindow().hasFocus()) return;
        
        auto track = app->getTrack(trackName);
        if (!track) return;

        app->setSelectedTrack(trackName);
        placementState.currentSelectedTrack = trackName;
        
        bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        
        if (ctrlPressed) {
            placementState.isDraggingDeletion = true;
            placementState.isDraggingPlacement = false;
            placementState.processedPositions.clear();
            
            sf::Vector2f globalMousePos = app->ui->getMousePosition();
            auto* trackRow = containers[trackName + "_scrollable_row"];
            if (trackRow) {
                sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();
                processClipAtPosition(track, localMousePos, true);
            }
        } else {
            sf::Vector2f globalMousePos = app->ui->getMousePosition();
            auto* trackRow = containers[trackName + "_scrollable_row"];
            if (trackRow) {
                sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();
                float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineState.timelineOffset, timelineState.timelineOffset);
                timelineState.virtualCursorTime = timePosition;
                timelineState.showVirtualCursor = true;
                if (!app->isPlaying()) {
                    app->setPosition(timelineState.virtualCursorTime);
                }
                app->setSavedPosition(timelineState.virtualCursorTime);
            }
        }
    };

    scrollableRowElement->m_modifier.onLClick([=]() { handleTrackLeftClick(); });
    scrollableRowElement->m_modifier.onRClick([=]() { handleTrackRightClick(); });

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
                    uiElements.trackRemoveButtons[trackName],
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

                    uiElements.trackMuteButtons[trackName],
                    
                    spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
                    
                    uiElements.trackSoloButtons[trackName]
                }),
            }),

            uiElements.trackVolumeSliders[trackName],

            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),

        spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
    }, trackName + "_label");

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
    static std::string prevSelectedTrack = "";
    
    const bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    handleDragOperations();
    handleClipSelection();
    
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

    float newMasterOffset = timelineState.timelineOffset;

    const double bpm = app->getBpm();
    const float zoomLevel = app->uiState.timelineZoomLevel;
    const float beatWidth = 100.f * zoomLevel;
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const bool isPlaying = app->isPlaying();
    const sf::Vector2f mousePos = app->ui->getMousePosition();

    // Find offset from any scrollable row
    for (const auto& track : allTracks) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        auto rowIt = containers.find(rowKey);
        if (rowIt == containers.end()) continue;
        
        auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
        if (scrollableRow->getOffset() != timelineState.timelineOffset) {
            newMasterOffset = scrollableRow->getOffset();
            break;
        }
    }

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
            constexpr float followSpeedPerSecond = 800.0f;
            const float followSpeed = followSpeedPerSecond * timelineState.deltaTime;
            const float offsetDelta = (targetOffset - newMasterOffset) * std::min(followSpeed, 1.0f);
            newMasterOffset = newMasterOffset + offsetDelta;
        }
    }

    const float clampedOffset = std::min(0.f, newMasterOffset);

    const bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    constexpr float baseScrollSpeedPerSecond = 1800.0f;
    const float frameRateIndependentScrollSpeed = ctrlHeld ? 0.0f : baseScrollSpeedPerSecond * timelineState.deltaTime;

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

            auto [timeSigNum, timeSigDen] = app->getTimeSignature();
            auto lines = generateTimelineMeasures(beatWidth, clampedOffset, trackRow->getSize(), timeSigNum, timeSigDen, &app->resources);
            
            std::vector<std::shared_ptr<sf::Drawable>> clips;
            
            if (track->getType() == Track::TrackType::MIDI) {
                MIDITrack* midiTrack = static_cast<MIDITrack*>(track.get());
                const auto& midiClipsVec = midiTrack->getMIDIClips();
                const sf::Vector2f localMousePos = mousePos - trackRowPos;
                
                for (size_t i = 0; i < midiClipsVec.size(); ++i) {
                    const auto& mc = midiClipsVec[i];
                    const float clipWidthPixels = mc.duration * pixelsPerSecond;
                    const float clipXPosition = (mc.startTime * pixelsPerSecond) + clampedOffset;
                    const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
                    
                    if (clipRect.contains(localMousePos)) {
                        if (!app->getWindow().hasFocus()) continue;
                        if (!ctrlPressed && !prevCtrlPressed && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && features.enableMouseInput) {
                            if (!dragState.isDraggingClip && !dragState.clipSelectedForDrag) {
                                selectedMIDIClipInfo.hasSelection = true;
                                selectedMIDIClipInfo.startTime = mc.startTime;
                                selectedMIDIClipInfo.duration = mc.duration;
                                selectedMIDIClipInfo.trackName = track->getName();
                                selectedClip = nullptr;
                                DEBUG_PRINT("[TIMELINE] Selected MIDI clip: startTime=" << mc.startTime << ", duration=" << mc.duration);
                                app->setSelectedTrack(track->getName());
                                
                                float midiMouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                
                                bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                                
                                if (!shiftHeld) {
                                    const double beatDuration = 60.0 / app->getBpm();
                                    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                                    const double subBeatDuration = beatDuration / static_cast<double>(timeSigDen);
                                    double snappedTime = std::floor(midiMouseTimeInTrack / subBeatDuration) * subBeatDuration;
                                    timelineState.virtualCursorTime = std::max(0.0, snappedTime);
                                } else {
                                    timelineState.virtualCursorTime = std::max(0.0, static_cast<double>(midiMouseTimeInTrack));
                                }
                                
                                timelineState.showVirtualCursor = true;
                                timelineState.virtualCursorVisible = true;
                                timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                                
                                if (!app->isPlaying()) {
                                    app->setPosition(timelineState.virtualCursorTime);
                                }
                                app->setSavedPosition(timelineState.virtualCursorTime);
                                
                                dragState.clipSelectedForDrag = true;
                                dragState.draggedMIDIClip = const_cast<MIDIClip*>(&mc);  // Use the current clip directly
                                dragState.draggedAudioClip = nullptr;
                                dragState.dragStartMousePos = mousePos;
                                dragState.dragStartClipTime = mc.startTime;
                                dragState.dragMouseOffsetInClip = midiMouseTimeInTrack - mc.startTime;
                                dragState.draggedTrackRowPos = trackRowPos;
                                dragState.draggedTrackName = track->getName();
                            }
                        }
                    }
                }
                
                // Convert MIDI clips to AudioClips for visualization (reuse existing function)
                std::vector<AudioClip> tempAudioClips;
                tempAudioClips.reserve(midiClipsVec.size());
                AudioClip* tempSelectedClip = nullptr;
                
                for (size_t i = 0; i < midiClipsVec.size(); ++i) {
                    const auto& mc = midiClipsVec[i];
                    AudioClip tempClip;
                    tempClip.startTime = mc.startTime;
                    tempClip.duration = mc.duration;
                    tempClip.sourceFile = juce::File();
                    tempAudioClips.push_back(tempClip);
                    
                    if (selectedMIDIClipInfo.hasSelection && 
                        std::abs(mc.startTime - selectedMIDIClipInfo.startTime) < 0.001 &&
                        std::abs(mc.duration - selectedMIDIClipInfo.duration) < 0.001) {
                        tempSelectedClip = &tempAudioClips.back();
                    }
                }
                
                clips = generateClipRects(bpm, beatWidth, clampedOffset, trackRow->getSize(), 
                                        tempAudioClips, 0.f, &app->resources, &app->uiState, tempSelectedClip, 
                                        track->getName(), currentSelectedTrack);
            } else {
                const auto& clipsVec = track->getClips();
                const sf::Vector2f localMousePos = mousePos - trackRowPos;
                
                for (size_t i = 0; i < clipsVec.size(); ++i) {
                    const auto& ac = clipsVec[i];
                    const float clipWidthPixels = ac.duration * pixelsPerSecond;
                    const float clipXPosition = (ac.startTime * pixelsPerSecond) + clampedOffset;
                    const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
                    
                    if (clipRect.contains(localMousePos)) {
                        if (!app->getWindow().hasFocus()) continue;
                        if (!ctrlPressed && !prevCtrlPressed && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && features.enableMouseInput) {
                            if (!dragState.isDraggingClip && !dragState.clipSelectedForDrag) {
                                selectedClip = const_cast<AudioClip*>(&clipsVec[i]);
                                selectedMIDIClipInfo.hasSelection = false;  // Clear MIDI clip selection
                                app->setSelectedTrack(track->getName());
                                
                                float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                
                                bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                                
                                if (!shiftHeld) {
                                    const double beatDuration = 60.0 / app->getBpm();
                                    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                                    const double subBeatDuration = beatDuration / static_cast<double>(timeSigDen);
                                    double snappedTime = std::floor(mouseTimeInTrack / subBeatDuration) * subBeatDuration;
                                    timelineState.virtualCursorTime = std::max(0.0, snappedTime);
                                } else {
                                    timelineState.virtualCursorTime = std::max(0.0, static_cast<double>(mouseTimeInTrack));
                                }
                                
                                timelineState.showVirtualCursor = true;
                                timelineState.virtualCursorVisible = true;
                                timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                                
                                if (!app->isPlaying()) {
                                    app->setPosition(timelineState.virtualCursorTime);
                                }
                                app->setSavedPosition(timelineState.virtualCursorTime);
                                
                                dragState.clipSelectedForDrag = true;
                                dragState.draggedAudioClip = selectedClip;
                                dragState.draggedMIDIClip = nullptr;
                                dragState.dragStartMousePos = mousePos;
                                dragState.dragStartClipTime = ac.startTime;
                                
                                // Calculate where within the clip the mouse initially clicked
                                float audioMouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                dragState.dragMouseOffsetInClip = audioMouseTimeInTrack - ac.startTime;
                                
                                dragState.draggedTrackRowPos = trackRowPos;
                                dragState.draggedTrackName = track->getName();
                            }
                        }
                    }
                }

                // Generate audio clip rectangles
                clips = generateClipRects(bpm, beatWidth, clampedOffset, trackRow->getSize(), 
                                        track->getClips(), 0.f, &app->resources, &app->uiState, selectedClip, 
                                        track->getName(), currentSelectedTrack);
            }

            // Handle empty timeline area clicks for virtual cursor
            if (!dragState.isDraggingClip && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && app->getWindow().hasFocus() && features.enableMouseInput) {
                const sf::Vector2f localMousePos = mousePos - trackRowPos;
                
                // Check if click is in timeline area but not on any clips
                bool clickedOnClip = false;
                
                // Check MIDI clips
                if (track->getType() == Track::TrackType::MIDI) {
                    MIDITrack* midiTrack = static_cast<MIDITrack*>(track.get());
                    const auto& midiClipsVec = midiTrack->getMIDIClips();
                    for (size_t i = 0; i < midiClipsVec.size(); ++i) {
                        const auto& mc = midiClipsVec[i];
                        const float clipWidthPixels = mc.duration * pixelsPerSecond;
                        const float clipXPosition = (mc.startTime * pixelsPerSecond) + clampedOffset;
                        const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
                        if (clipRect.contains(localMousePos)) {
                            clickedOnClip = true;
                            break;
                        }
                    }
                } else {
                    const auto& clipsVec = track->getClips();
                    for (size_t i = 0; i < clipsVec.size(); ++i) {
                        const auto& ac = clipsVec[i];
                        const float clipWidthPixels = ac.duration * pixelsPerSecond;
                        const float clipXPosition = (ac.startTime * pixelsPerSecond) + clampedOffset;
                        const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
                        if (clipRect.contains(localMousePos)) {
                            clickedOnClip = true;
                            break;
                        }
                    }
                }
                
                if (!clickedOnClip && localMousePos.x >= 0 && localMousePos.y >= 0 && localMousePos.y <= trackRow->getSize().y) {
                    float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                    
                    bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    
                    if (!shiftHeld) {
                        const double beatDuration = 60.0 / app->getBpm();
                        auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                        const double subBeatDuration = beatDuration / static_cast<double>(timeSigDen);
                        double snappedTime = std::floor(mouseTimeInTrack / subBeatDuration) * subBeatDuration;
                        timelineState.virtualCursorTime = std::max(0.0, snappedTime);
                    } else {
                        timelineState.virtualCursorTime = std::max(0.0, static_cast<double>(mouseTimeInTrack));
                    }
                    
                    timelineState.showVirtualCursor = true;
                    timelineState.virtualCursorVisible = true;
                    timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                    
                    if (!app->isPlaying()) {
                        app->setPosition(timelineState.virtualCursorTime);
                    }
                    app->setSavedPosition(timelineState.virtualCursorTime);
                    
                    selectedClip = nullptr;
                    selectedMIDIClipInfo.hasSelection = false;  // Clear MIDI clip selection
                    DEBUG_PRINT("[TIMELINE] Cleared clip selection (clicked empty area)");
                }
            }

            std::vector<std::shared_ptr<sf::Drawable>> rowGeometry;
            rowGeometry.reserve(clips.size() + lines.size() + 1);
            
            rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(clips.begin()), std::make_move_iterator(clips.end()));
            rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
            
            if (timelineState.showVirtualCursor && timelineState.virtualCursorVisible && track->getName() == currentSelectedTrack) {
                const float cursorXPosition = (timelineState.virtualCursorTime * pixelsPerSecond) + clampedOffset;
                const float cursorWidth = 5.0f;
                auto cursor = std::make_shared<sf::RectangleShape>(sf::Vector2f(cursorWidth, trackRow->getSize().y));
                cursor->setPosition({cursorXPosition - cursorWidth/2.f, 0.f});
                
                const sf::Color& clipColor = app->resources.activeTheme->clip_color;
                cursor->setFillColor(sf::Color(
                    255 - clipColor.r,
                    255 - clipColor.g,
                    255 - clipColor.b,
                    255
                ));
                
                rowGeometry.push_back(cursor);
            }
            
            scrollableRow->setCustomGeometry(rowGeometry);
        }
    }
    prevCtrlPressed = ctrlPressed;

    if (timelineState.showVirtualCursor && !app->isPlaying()) {
        app->setPosition(timelineState.virtualCursorTime);
    }

    if (app->getAllTracks().size() > 1) {
        std::vector<std::shared_ptr<sf::Drawable>> timelineGeometry;
        
        if (app->isPlaying()) {
            auto playhead = getPlayHead(
                app->getBpm(), 
                100.f * app->uiState.timelineZoomLevel,
                clampedOffset,
                app->getPosition(),
                timelineElement->getSize()
            );
            timelineGeometry.push_back(playhead);
        }
        
        timelineElement->setCustomGeometry(timelineGeometry);
        timelineState.timelineOffset = clampedOffset;
    }

    if (dragState.clipSelectedForDrag && !dragState.isDraggingClip) {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            const sf::Vector2f currentMousePos = app->ui->getMousePosition();
            const float dragDistance = std::sqrt(
                std::pow(currentMousePos.x - dragState.dragStartMousePos.x, 2) +
                std::pow(currentMousePos.y - dragState.dragStartMousePos.y, 2)
            );
            
            if (dragDistance > dragState.DRAG_THRESHOLD) {
                dragState.isDraggingClip = true;
                dragState.isDraggingAudioClip = (dragState.draggedAudioClip != nullptr);
                dragState.isDraggingMIDIClip = (dragState.draggedMIDIClip != nullptr);
                dragState.clipSelectedForDrag = false;
            }
        } else {
            dragState.clipSelectedForDrag = false;
        }
    }

    if (dragState.isDraggingClip) {
        if (app->ui->isMouseDragging()) {
            const sf::Vector2f currentMousePos = app->ui->getMousePosition();
            
            const sf::Vector2f currentLocalMousePos = currentMousePos - dragState.draggedTrackRowPos;
            
            const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
            const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
            
            float currentMouseTimeInTrack = (currentLocalMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
            double newStartTime = std::max(0.0, currentMouseTimeInTrack - dragState.dragMouseOffsetInClip);
            
            bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
            
            if (!shiftHeld) {
                const double beatDuration = 60.0 / app->getBpm();
                auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                const double subBeatDuration = beatDuration / static_cast<double>(timeSigDen);
                newStartTime = std::floor(newStartTime / subBeatDuration) * subBeatDuration;
            }
            
            if (dragState.isDraggingAudioClip && dragState.draggedAudioClip) {
                dragState.draggedAudioClip->startTime = newStartTime;
            } else if (dragState.isDraggingMIDIClip && dragState.draggedMIDIClip) {
                dragState.draggedMIDIClip->startTime = newStartTime;
            }
        } else {
            dragState.isDraggingClip = false;
            dragState.isDraggingAudioClip = false;
            dragState.isDraggingMIDIClip = false;
            dragState.clipSelectedForDrag = false;
            dragState.draggedAudioClip = nullptr;
            dragState.draggedMIDIClip = nullptr;
            dragState.draggedTrackName.clear();
        }
    }
}

void TimelineComponent::rebuildUI() {
    auto* baseTimelineColumn = getColumn("base_timeline_column");
    if (!baseTimelineColumn) return;

    baseTimelineColumn->clear();

    uiElements.masterTrackElement = masterTrack();
    
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
        uiElements.masterTrackElement
    });
}

void TimelineComponent::rebuildUIFromEngine() {
    if (!initialized) return;
    
    DEBUG_PRINT("Rebuilding UI from engine state");
    
    // Clear existing UI elements
    if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
        timelineIt->second->clear();
    }
    
    // Clear cached UI element references
    uiElements.trackMuteButtons.clear();
    uiElements.trackVolumeSliders.clear();
    uiElements.trackSoloButtons.clear(); 
    uiElements.trackRemoveButtons.clear();
    
    // Rebuild tracks from current engine state
    const auto& allTracks = app->getAllTracks();
    for (const auto& track : allTracks) {
        if (track->getName() == "Master") continue;
        
        if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
            timelineIt->second->addElements({
                spacer(Modifier().setfixedHeight(4.f)),
                this->track(track->getName(), Align::TOP | Align::LEFT, track->getVolume(), track->getPan())
            });
        }
        
        DEBUG_PRINT("Rebuilt track: " << track->getName());
    }
    
    // Sync UI elements with current engine state
    syncSlidersToEngine();
}

void TimelineComponent::syncSlidersToEngine() {
    if (app->getMasterTrack() && uiElements.masterVolumeSlider) {
        float engineVol = app->getMasterTrack()->getVolume();
        float sliderValue = decibelsToFloat(engineVol);
        uiElements.masterVolumeSlider->setValue(sliderValue);
    }
    
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        if (auto volumeSlider = uiElements.trackVolumeSliders.find(track->getName());
            volumeSlider != uiElements.trackVolumeSliders.end() && volumeSlider->second) {
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
    std::vector<std::shared_ptr<sf::Drawable>> selectedClipDrawables;
    clipRects.reserve(clips.size() * 2);
    
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const sf::Color& clipColor = resources->activeTheme->clip_color;

    for (const auto& ac : clips) {
        const float clipWidthPixels = ac.duration * pixelsPerSecond;
        const float clipXPosition = (ac.startTime * pixelsPerSecond) + scrollOffset;

        // Safe comparison for selection highlighting
        bool isSelected = false;
        if (selectedClip && currentTrackName == selectedTrackName) {
            // Compare numerical values first (safer)
            bool timesMatch = (ac.startTime == selectedClip->startTime);
            bool durationsMatch = (ac.duration == selectedClip->duration);
            
            // Only compare files if times and durations match (reduces risk)
            bool filesMatch = false;
            if (timesMatch && durationsMatch) {
                try {
                    // Attempt safe comparison by comparing strings, with fallback
                    std::string clipPath, selectedPath;
                    try {
                        clipPath = ac.sourceFile.getFullPathName().toStdString();
                        selectedPath = selectedClip->sourceFile.getFullPathName().toStdString();
                        filesMatch = (clipPath == selectedPath);
                    } catch (...) {
                        // If string conversion fails, just use pointer equality as fallback
                        filesMatch = (&ac == selectedClip);
                    }
                } catch (...) {
                    // If any comparison fails, assume different clips
                    filesMatch = false;
                }
            }
            
            isSelected = timesMatch && durationsMatch && filesMatch;
        }

        std::vector<std::shared_ptr<sf::Drawable>>* targetContainer = isSelected ? &selectedClipDrawables : &clipRects;

        if (isSelected) {
            auto outlineRect = std::make_shared<sf::RectangleShape>();
            outlineRect->setSize({clipWidthPixels, rowSize.y});
            outlineRect->setPosition({clipXPosition, 0.f});
            outlineRect->setFillColor(sf::Color(
                255 - clipColor.r,
                255 - clipColor.g,
                255 - clipColor.b
            ));
            targetContainer->emplace_back(std::move(outlineRect));

            auto clipRect = std::make_shared<sf::RectangleShape>();
            const float insetThickness = 3.f;
            clipRect->setSize({clipWidthPixels - 2 * insetThickness, rowSize.y - 2 * insetThickness});
            clipRect->setPosition({clipXPosition + insetThickness, insetThickness});
            clipRect->setFillColor(clipColor);
            targetContainer->emplace_back(std::move(clipRect));
        } else {
            auto clipRect = std::make_shared<sf::RectangleShape>();
            clipRect->setSize({clipWidthPixels, rowSize.y});
            clipRect->setPosition({clipXPosition, 0.f});
            clipRect->setFillColor(clipColor);
            targetContainer->emplace_back(std::move(clipRect));
        }

        auto waveformDrawables = generateWaveformData(
            ac,
            sf::Vector2f(clipXPosition, 0.f),
            sf::Vector2f(clipWidthPixels, rowSize.y),
            verticalOffset,
            resources,
            uiState
        );
        
        targetContainer->insert(targetContainer->end(),
                        std::make_move_iterator(waveformDrawables.begin()),
                        std::make_move_iterator(waveformDrawables.end()));
    }
    
    // Add selected clip drawables last so they render on top
    clipRects.insert(clipRects.end(),
                    std::make_move_iterator(selectedClipDrawables.begin()),
                    std::make_move_iterator(selectedClipDrawables.end()));
    
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
    
    float closestLeftX = 0.0f; // Always round down - find the measure line to the left
    
    for (const auto& line : lines) {
        if (const auto rect = std::dynamic_pointer_cast<const sf::RectangleShape>(line)) {
            const float lineX = rect->getPosition().x;
            // Only consider lines that are to the left of or at the click position
            if (lineX <= pos.x && lineX > closestLeftX) {
                closestLeftX = lineX;
            }
        }
    }
    
    return closestLeftX;
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

void TimelineComponent::processClipAtPosition(Track* track, const sf::Vector2f& localMousePos, bool isRightClick) {
    if (!track) return;
    
    float timePosition;
    
    if (isRightClick) {
        timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineState.timelineOffset, timelineState.timelineOffset);
    } else {
        auto* trackRow = containers[track->getName() + "_scrollable_row"];
        if (!trackRow) return;
        auto [timeSigNum, timeSigDen] = app->getTimeSignature();
        auto lines = generateTimelineMeasures(100.f * app->uiState.timelineZoomLevel, timelineState.timelineOffset, trackRow->getSize(), timeSigNum, timeSigDen, &app->resources);
        float snapX = getNearestMeasureX(localMousePos, lines);
        timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, snapX - timelineState.timelineOffset, timelineState.timelineOffset);
    }
    
    double roundedPosition = std::floor(timePosition * 100.0) / 100.0;
    if (placementState.processedPositions.count(roundedPosition)) return;
    placementState.processedPositions.insert(roundedPosition);
    
    if (isRightClick) {
        if (track->getType() == Track::TrackType::MIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            const auto& midiClips = midiTrack->getMIDIClips();
            for (size_t i = 0; i < midiClips.size(); ++i) {
                if (timePosition >= midiClips[i].startTime && timePosition <= (midiClips[i].startTime + midiClips[i].duration)) {
                    midiTrack->removeMIDIClip(i);
                    break;
                }
            }
        } else {
            const auto& clips = track->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                if (timePosition >= clips[i].startTime && timePosition <= (clips[i].startTime + clips[i].duration)) {
                    track->removeClip(i);
                    break;
                }
            }
        }
    } else {
        if (track->getType() == Track::TrackType::MIDI) {
            double beatDuration = 60.0 / app->getBpm();
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            
            const auto& existingMIDIClips = midiTrack->getMIDIClips();
            bool collision = false;
            double newEndTime = timePosition + beatDuration;
            for (const auto& existingClip : existingMIDIClips) {
                double existingEndTime = existingClip.startTime + existingClip.duration;
                if (!(newEndTime <= existingClip.startTime || timePosition >= existingEndTime)) {
                    collision = true;
                    break;
                }
            }
            
            if (!collision) {
                MIDIClip newMIDIClip(timePosition, beatDuration, 1, 1.0f);
                midiTrack->addMIDIClip(newMIDIClip);
                
                const auto& midiClips = midiTrack->getMIDIClips();
                if (!midiClips.empty()) {
                    const auto& newClip = midiClips.back();
                    selectedMIDIClipInfo.hasSelection = true;
                    selectedMIDIClipInfo.startTime = newClip.startTime;
                    selectedMIDIClipInfo.duration = newClip.duration;
                    selectedMIDIClipInfo.trackName = track->getName();
                    selectedClip = nullptr;
                    app->setSelectedTrack(track->getName());
                    
                    timelineState.virtualCursorTime = timePosition;
                    timelineState.showVirtualCursor = true;
                    timelineState.virtualCursorVisible = true;
                    timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                }
            }
        } else {
            if (track->getReferenceClip()) {
                AudioClip* refClip = track->getReferenceClip();
                
                const auto& existingClips = track->getClips();
                bool collision = false;
                double newEndTime = timePosition + refClip->duration;
                for (const auto& existingClip : existingClips) {
                    double existingEndTime = existingClip.startTime + existingClip.duration;
                    if (!(newEndTime <= existingClip.startTime || timePosition >= existingEndTime)) {
                        collision = true;
                        break;
                    }
                }
                
                if (!collision) {
                    track->addClip(AudioClip(refClip->sourceFile, timePosition, 0.0, refClip->duration, 1.0f));
                    
                    const auto& audioClips = track->getClips();
                    if (!audioClips.empty()) {
                        selectedClip = const_cast<AudioClip*>(&audioClips.back());
                        selectedMIDIClipInfo.hasSelection = false;  // Clear MIDI clip selection
                        app->setSelectedTrack(track->getName());
                        
                        timelineState.virtualCursorTime = timePosition;
                        timelineState.showVirtualCursor = true;
                        timelineState.virtualCursorVisible = true;
                        timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                    }
                }
            }
        }
    }
}

void TimelineComponent::handleDragOperations() {
    if (!features.enableClipDragging && !features.enableClipPlacement && !features.enableClipDeletion) return;
    
    // Process all drag operations through organized subsystem functions
    if (features.enableClipDragging) {
        processDragOperations();
    }
    
    // Update drag state for active operations
    updateDragState();
}

void TimelineComponent::handleClipSelection() {
    static bool prevBackspace = false;
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
    
    if ((selectedClip || selectedMIDIClipInfo.hasSelection) && backspace && !prevBackspace && app->getWindow().hasFocus()) {
        const std::string currentSelectedTrack = app->getSelectedTrack();
        
        for (auto& t : app->getAllTracks()) {
            if (t->getName() != currentSelectedTrack) continue;
            
            // Handle MIDI clip deletion
            if (selectedMIDIClipInfo.hasSelection && t->getType() == Track::TrackType::MIDI) {
                MIDITrack* midiTrack = static_cast<MIDITrack*>(t.get());
                const auto& midiClips = midiTrack->getMIDIClips();
                
                for (size_t i = 0; i < midiClips.size(); ++i) {
                    const auto& clip = midiClips[i];
                    if (std::abs(clip.startTime - selectedMIDIClipInfo.startTime) < 0.001 &&
                        std::abs(clip.duration - selectedMIDIClipInfo.duration) < 0.001) {
                        midiTrack->removeMIDIClip(i);
                        selectedMIDIClipInfo.hasSelection = false;  // Clear selection
                        break;
                    }
                }
            }
            // Handle audio clip deletion
            else if (selectedClip) {
                const auto& clips = t->getClips();
                
                for (size_t i = 0; i < clips.size(); ++i) {
                    const auto& clip = clips[i];
                    if (clip.startTime == selectedClip->startTime &&
                        clip.duration == selectedClip->duration &&
                        clip.sourceFile == selectedClip->sourceFile) {
                        t->removeClip(static_cast<int>(i));
                        selectedClip = nullptr;
                        break;
                    }
                }
            }
            break;
        }
    }
    prevBackspace = backspace;
}

void TimelineComponent::handleAllUserInput() {
    if (app->ui->isInputBlocked()) return;
    if (!features.enableKeyboardInput && !features.enableMouseInput) return;
    
    if (features.enableKeyboardInput) {
        handleKeyboardInput();
    }
    
    if (features.enableMouseInput) {
        handleMouseInput();
    }
}

void TimelineComponent::handleKeyboardInput() {
    static bool prevCtrl = false, prevPlus = false, prevMinus = false, prevBackspace = false;
    static bool prevC = false, prevV = false, prevD = false;
    
    const bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    const bool plus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Equal);
    const bool minus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Hyphen);
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
    const bool c = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C);
    const bool v = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);
    const bool d = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);

    // Block keyboard input when window doesn't have focus
    if (!app->getWindow().hasFocus()) {
        prevCtrl = ctrl;
        prevPlus = plus;
        prevMinus = minus;
        prevBackspace = backspace;
        prevC = c;
        prevV = v;
        prevD = d;
        return;
    }

    if (ctrl && c && !prevC) {
        copySelectedClips();
    }
    if (ctrl && v && !prevV) {
        pasteClips();
    }
    if (ctrl && d && !prevD) {
        duplicateSelectedClips();
    }

    // Handle clip deletion
    if (selectedClip && backspace && !prevBackspace) {
        const auto& allTracks = app->getAllTracks();
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
                    return;
                }
            }
        }
    }

    prevCtrl = ctrl;
    prevPlus = plus;
    prevMinus = minus;
    prevBackspace = backspace;
    prevC = c;
    prevV = v;
    prevD = d;
}

void TimelineComponent::handleMouseInput() {
    if (!features.enableMouseInput || !app->getWindow().hasFocus()) return;
    
    const sf::Vector2f mousePos = app->ui->getMousePosition();
    const bool isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    const bool isRightPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    const bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    // Handle scroll-based zoom (only when Ctrl is held)
    if (ctrlPressed) {
        const float verticalDelta = app->ui->getVerticalScrollDelta();
        if (verticalDelta != 0.f) {
            constexpr float maxZoom = 5.0f;
            constexpr float minZoom = 0.1f;
            
            float currentZoom = app->uiState.timelineZoomLevel;
            float normalizedZoom = (currentZoom - minZoom) / (maxZoom - minZoom);
            
            float baseSpeed = 0.08f;
            float speedMultiplier = 1.0f;
            
            if (normalizedZoom < 0.25f) {
                float nearMinFactor = normalizedZoom / 0.25f;
                speedMultiplier = 0.2f + (nearMinFactor * 0.8f);
            }
            else if (normalizedZoom > 0.88f) {
                float nearMaxFactor = (normalizedZoom - 0.88f) / 0.12f;
                speedMultiplier = 1.0f - (nearMaxFactor * 0.5f);
            }
            
            float adaptiveZoomSpeed = baseSpeed * speedMultiplier;
            adaptiveZoomSpeed = std::max(0.015f, adaptiveZoomSpeed);
            
            float newZoom = currentZoom + (verticalDelta > 0 ? adaptiveZoomSpeed : -adaptiveZoomSpeed);
            newZoom = std::clamp(newZoom, minZoom, maxZoom);
            
            if (newZoom != app->uiState.timelineZoomLevel) {
                handleZoomChange(newZoom);
                app->ui->resetScrollDeltas();
            }
        }
    }
    
    // Handle drag operations if mouse is being dragged
    if (app->ui->isMouseDragging()) {
        if (features.enableClipDragging) {
            processDragOperations();
        }
    }
}

void TimelineComponent::handleZoomChange(float newZoom) {
    const float currentOffset = timelineState.timelineOffset;
    const float oldZoom = app->uiState.timelineZoomLevel;
    const sf::Vector2f mousePos = app->ui->getMousePosition();
    
    // Use the timeline container's position as the consistent coordinate reference
    sf::Vector2f timelinePos(0.f, 0.f);
    if (auto timelineContainer = containers.find("timeline"); timelineContainer != containers.end() && timelineContainer->second) {
        timelinePos = timelineContainer->second->getPosition();
    }
    
    // Convert mouse position to local coordinates within the timeline component
    const sf::Vector2f localMousePos = mousePos - timelinePos;
    
    // Calculate what timeline time is currently under the mouse cursor
    const float timeAtMouse = xPosToSeconds(app->getBpm(), 100.f * oldZoom, localMousePos.x - currentOffset, 0.f);
    
    app->uiState.timelineZoomLevel = newZoom;
    
    // Calculate new offset so that same timeline time is under mouse cursor
    const float pixelsPerSecond = (100.f * newZoom * app->getBpm()) / 60.0f;
    timelineState.timelineOffset = localMousePos.x - (timeAtMouse * pixelsPerSecond);
    
    for (const auto& track : app->getAllTracks()) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
            scrollableRow->setOffset(std::min(0.f, timelineState.timelineOffset));
        }
    }
}

void TimelineComponent::syncUIToEngine() {
    handleMasterTrackControls();
    handleTrackControls();
    updateTrackHighlighting();
}

void TimelineComponent::handleMasterTrackControls() {
    // Handle master mute button
    if (uiElements.muteMasterButton && uiElements.muteMasterButton->isClicked() && app->getWindow().hasFocus()) {
        auto* masterTrack = app->getMasterTrack();
        masterTrack->toggleMute();
        uiElements.muteMasterButton->m_modifier.setColor(
            masterTrack->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
        );
        uiElements.muteMasterButton->setClicked(false);
    }
    
    // Handle master volume slider
    if (this->isVisible() && uiElements.masterVolumeSlider) {
        const float newMasterVolDb = floatToDecibels(uiElements.masterVolumeSlider->getValue());
        auto* masterTrack = app->getMasterTrack();
        constexpr float volumeTolerance = 0.001f;
        
        if (std::abs(masterTrack->getVolume() - newMasterVolDb) > volumeTolerance) {
            masterTrack->setVolume(newMasterVolDb);
        }
    }
}

void TimelineComponent::handleTrackControls() {
    const auto& allTracks = app->getAllTracks();
    
    // Check if track lists are synchronized
    std::set<std::string> engineTrackNames, uiTrackNames;
    
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name != "Master") {
            engineTrackNames.emplace(name);
        }
    }
    
    for (const auto& [name, _] : uiElements.trackMuteButtons) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : uiElements.trackVolumeSliders) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : uiElements.trackSoloButtons) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : uiElements.trackRemoveButtons) {
        uiTrackNames.emplace(name);
    }
    
    // Clear UI elements if tracks don't match
    if (engineTrackNames != uiTrackNames) {
        if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
            timelineIt->second->clear();
        }
        uiElements.trackMuteButtons.clear();
        uiElements.trackVolumeSliders.clear();
        uiElements.trackSoloButtons.clear();
        uiElements.trackRemoveButtons.clear();
    }
    
    // Handle individual track controls
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name == "Master") continue;
        
        handleIndividualTrackControls(t.get(), name);
    }
}

void TimelineComponent::handleIndividualTrackControls(Track* track, const std::string& name) {
    const bool hasMuteButton = uiElements.trackMuteButtons.count(name);
    const bool hasVolumeSlider = uiElements.trackVolumeSliders.count(name);
    const bool hasSoloButton = uiElements.trackSoloButtons.count(name);
    const bool hasRemoveButton = uiElements.trackRemoveButtons.count(name);
    
    // Create track UI if it doesn't exist
    if (!hasMuteButton && !hasVolumeSlider && !hasSoloButton && !hasRemoveButton) {
        if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
            timelineIt->second->addElements({
                spacer(Modifier().setfixedHeight(4.f)),
                this->track(name, Align::TOP | Align::LEFT, track->getVolume(), track->getPan())
            });
        }
    }
    
    // Handle mute button
    if (auto muteBtnIt = uiElements.trackMuteButtons.find(name); 
        muteBtnIt != uiElements.trackMuteButtons.end() && muteBtnIt->second && muteBtnIt->second->isClicked() && app->getWindow().hasFocus()) {
        track->toggleMute();
        muteBtnIt->second->m_modifier.setColor(
            track->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
        );
        muteBtnIt->second->setClicked(false);
    }
    
    // Handle solo button
    if (auto soloBtnIt = uiElements.trackSoloButtons.find(name); 
        soloBtnIt != uiElements.trackSoloButtons.end() && soloBtnIt->second && soloBtnIt->second->isClicked() && app->getWindow().hasFocus()) {
        track->setSolo(!track->isSolo());
        soloBtnIt->second->m_modifier.setColor(
            track->isSolo() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
        );
        soloBtnIt->second->setClicked(false);
    }
    
    // Handle volume slider
    if (this->isVisible()) {
        if (auto sliderIt = uiElements.trackVolumeSliders.find(name); 
            sliderIt != uiElements.trackVolumeSliders.end() && sliderIt->second) {
            const float sliderDb = floatToDecibels(sliderIt->second->getValue());
            constexpr float volumeTolerance = 0.001f;
            if (std::abs(track->getVolume() - sliderDb) > volumeTolerance) {
                track->setVolume(sliderDb);
            }
        }
    }
}

void TimelineComponent::updateTrackHighlighting() {
    const std::string selectedTrack = app->getSelectedTrack();
    const auto& allTracks = app->getAllTracks();
    
    // Update regular track highlighting
    for (const auto& t : allTracks) {
        if (t->getName() == "Master") continue;
        
        const std::string labelKey = t->getName() + "_label";
        if (auto labelIt = containers.find(labelKey); labelIt != containers.end() && labelIt->second) {
            auto* labelColumn = labelIt->second;
            if (t->getName() == selectedTrack) {
                labelColumn->m_modifier.setColor(app->resources.activeTheme->selected_track_color);
            } else {
                labelColumn->m_modifier.setColor(app->resources.activeTheme->track_color);
            }
        }
    }

    // Handle Master track highlighting
    const std::string masterLabelKey = "Master_Track_Column";
    if (auto masterIt = containers.find(masterLabelKey); masterIt != containers.end() && masterIt->second) {
        auto* masterColumn = masterIt->second;
        if (selectedTrack == "Master") {
            masterColumn->m_modifier.setColor(app->resources.activeTheme->selected_track_color);
        } else {
            masterColumn->m_modifier.setColor(app->resources.activeTheme->master_track_color);
        }
    }
}

void TimelineComponent::processDragOperations() {
    if (!features.enableClipDragging) return;
    
    handleClipDragOperations();
    if (features.enableClipPlacement) {
        handlePlacementDragOperations();
    }
    if (features.enableClipDeletion) {
        handleDeletionDragOperations();
    }
}

void TimelineComponent::handleClipDragOperations() {
    if (!features.enableClipDragging) return;
    
    bool isRightPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    // Handle traditional clip dragging (move clips around)
    if (dragState.isDraggingClip || dragState.isDraggingAudioClip || dragState.isDraggingMIDIClip) {
        if (isLeftPressed && !ctrlPressed) {
            // Continue dragging existing clip
            sf::Vector2f currentMousePos = app->ui->getMousePosition();
            sf::Vector2f dragDelta = currentMousePos - dragState.dragStartMousePos;
            
            // Update clip position based on drag delta
            // This would contain the actual clip position update logic
        } else {
            // End drag operation
            resetDragState();
        }
    }
}

void TimelineComponent::handlePlacementDragOperations() {
    if (!features.enableClipPlacement) return;
    
    bool isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    // Continue left-click drag placement
    if (placementState.isDraggingPlacement && isLeftPressed && ctrlPressed) {
        if (!placementState.currentSelectedTrack.empty()) {
            auto track = app->getTrack(placementState.currentSelectedTrack);
            if (track) {
                sf::Vector2f globalMousePos = app->ui->getMousePosition();
                auto* trackRow = containers[placementState.currentSelectedTrack + "_scrollable_row"];
                if (trackRow) {
                    sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();
                    processClipAtPosition(track, localMousePos, false); // false for placement
                }
            }
        }
    }
    
    // Handle new drag placement detection
    if (app->ui->isMouseDragging() && ctrlPressed && isLeftPressed && !placementState.isDraggingPlacement) {
        placementState.isDraggingPlacement = true;
        placementState.isDraggingDeletion = false;
    }
    
    // End placement drag when conditions no longer met
    if (placementState.isDraggingPlacement && (!isLeftPressed || !ctrlPressed)) {
        placementState.isDraggingPlacement = false;
        placementState.processedPositions.clear();
    }
}

void TimelineComponent::handleDeletionDragOperations() {
    if (!features.enableClipDeletion) return;
    
    bool isRightPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    // Continue right-click drag deletion
    if (placementState.isDraggingDeletion && isRightPressed && ctrlPressed) {
        if (!placementState.currentSelectedTrack.empty()) {
            auto track = app->getTrack(placementState.currentSelectedTrack);
            if (track) {
                sf::Vector2f globalMousePos = app->ui->getMousePosition();
                auto* trackRow = containers[placementState.currentSelectedTrack + "_scrollable_row"];
                if (trackRow) {
                    sf::Vector2f localMousePos = globalMousePos - trackRow->getPosition();
                    processClipAtPosition(track, localMousePos, true); // true for deletion
                }
            }
        }
    }
    
    // Handle new drag deletion detection  
    if (app->ui->isMouseDragging() && ctrlPressed && isRightPressed && !placementState.isDraggingDeletion) {
        placementState.isDraggingDeletion = true;
        placementState.isDraggingPlacement = false;
    }
    
    // End deletion drag when conditions no longer met
    if (placementState.isDraggingDeletion && (!isRightPressed || !ctrlPressed)) {
        placementState.isDraggingDeletion = false;
        placementState.processedPositions.clear();
    }
}

void TimelineComponent::updateDragState() {
    if (!features.enableClipDragging) return;
    
    sf::Vector2f currentMousePos = app->ui->getMousePosition();
    
    // Update drag positions if currently dragging
    if (dragState.isDraggingClip || dragState.isDraggingAudioClip || dragState.isDraggingMIDIClip) {
        // Calculate time position from mouse position
        const double bpm = app->getBpm();
        const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
        
        // Update drag state with current mouse position
        float mouseXInTimeline = currentMousePos.x - dragState.draggedTrackRowPos.x;
        double newClipTime = xPosToSeconds(bpm, beatWidth, mouseXInTimeline, timelineState.timelineOffset);
        
        // Apply grid snapping if enabled
        if (placementState.currentSelectedTrack == dragState.draggedTrackName) {
            auto [timeSigNum, timeSigDen] = app->getTimeSignature();
            double beatsPerMeasure = static_cast<double>(timeSigNum);
            double secondsPerMeasure = (beatsPerMeasure * 60.0) / bpm;
            newClipTime = std::round(newClipTime / secondsPerMeasure) * secondsPerMeasure;
        }
    }
}

void TimelineComponent::updateTimelineVisuals() {
    if (!this->isVisible()) return;
    
    updateScrolling();
    if (features.enableAutoFollow) {
        updatePlayheadFollowing();
    }
    renderTrackContent();
    if (features.enableVirtualCursor) {
        updateVirtualCursor();
    }
}

void TimelineComponent::updateScrolling() {
    if (!this->isVisible()) return;
    
    // Let UILO handle scrolling - read from scrollableRows instead of forcing offsets
    const auto& allTracks = app->getAllTracks();
    if (!allTracks.empty()) {
        // Use first track as the master offset source
        const std::string rowKey = allTracks[0]->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            auto* firstScrollableRow = static_cast<ScrollableRow*>(rowIt->second);
            float newMasterOffset = firstScrollableRow->getOffset();
            
            // Only update if there's been a significant change
            if (std::abs(newMasterOffset - timelineState.timelineOffset) > 0.1f) {
                timelineState.timelineOffset = newMasterOffset;
                
                // Synchronize all other rows to match the first row
                for (size_t i = 1; i < allTracks.size(); ++i) {
                    const std::string otherRowKey = allTracks[i]->getName() + "_scrollable_row";
                    if (auto otherRowIt = containers.find(otherRowKey); otherRowIt != containers.end() && otherRowIt->second) {
                        auto* scrollableRow = static_cast<ScrollableRow*>(otherRowIt->second);
                        scrollableRow->setOffset(newMasterOffset);
                    }
                }
            }
        }
    }
}

void TimelineComponent::updatePlayheadFollowing() {
    if (!features.enableAutoFollow || !app->isPlaying()) return;
    
    const double bpm = app->getBpm();
    const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
    const float playheadXPos = secondsToXPosition(bpm, beatWidth, std::round(app->getPosition() * 1000.0) / 1000.0);
    
    // Auto-follow playhead by adjusting timeline offset
    float visibleWidth = 0.f;
    const auto& allTracks = app->getAllTracks();
    
    for (const auto& track : allTracks) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            visibleWidth = rowIt->second->getSize().x;
            break;
        }
    }
    
    if (visibleWidth > 0.f) {
        const float followMargin = visibleWidth * 0.1f; // 10% margin
        const float currentPlayheadScreenPos = playheadXPos + timelineState.timelineOffset;
        
        if (currentPlayheadScreenPos > visibleWidth - followMargin) {
            timelineState.timelineOffset = visibleWidth - followMargin - playheadXPos;
            updateScrolling(); // Apply the new offset
            DEBUG_PRINT("Auto-following playhead - New offset: " << timelineState.timelineOffset);
        }
    }
}

void TimelineComponent::renderTrackContent() {
    if (!this->isVisible()) return;
    
    // Clear cached content if zoom or offset changed significantly  
    const float zoomThreshold = 0.1f;
    const float offsetThreshold = 50.0f;
    
    if (std::abs(cacheState.lastScrollOffset - timelineState.timelineOffset) > offsetThreshold) {
        // Invalidate cache due to significant scroll change
        cacheState.cachedMeasureLines.clear();
        cacheState.lastScrollOffset = timelineState.timelineOffset;
        DEBUG_PRINT("Invalidated render cache due to scroll change");
    }
    
    // Cache management for better performance
    const double bpm = app->getBpm();
    const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
    
    if (cacheState.lastMeasureWidth < 0.f || std::abs(cacheState.lastMeasureWidth - beatWidth) > zoomThreshold) {
        cacheState.cachedMeasureLines.clear();
        cacheState.lastMeasureWidth = beatWidth;
        DEBUG_PRINT("Invalidated render cache due to zoom change");
    }
}

void TimelineComponent::updateVirtualCursor() {
    if (!features.enableVirtualCursor) {
        timelineState.showVirtualCursor = false;
        return;
    }
    
    auto currentTime = std::chrono::steady_clock::now();
    auto blinkDuration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - timelineState.lastBlinkTime);
    if (blinkDuration.count() >= 500) {
        timelineState.virtualCursorVisible = !timelineState.virtualCursorVisible;
        timelineState.lastBlinkTime = currentTime;
    }
    
    const sf::Vector2f mousePos = app->ui->getMousePosition();
    const bool isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    static bool wasLeftPressed = false;
    
    bool justClicked = isLeftPressed && !wasLeftPressed;
    
    if (justClicked) {
        const auto& allTracks = app->getAllTracks();
        for (const auto& track : allTracks) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                const sf::Vector2f trackPos = rowIt->second->getPosition();
                const sf::Vector2f trackSize = rowIt->second->getSize();
                
                if (mousePos.x >= trackPos.x && mousePos.x <= trackPos.x + trackSize.x &&
                    mousePos.y >= trackPos.y && mousePos.y <= trackPos.y + trackSize.y) {
                    
                    const double bpm = app->getBpm();
                    const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
                    float mouseXInTimeline = mousePos.x - trackPos.x;
                    double rawTime = xPosToSeconds(bpm, beatWidth, mouseXInTimeline - timelineState.timelineOffset, timelineState.timelineOffset);
                    
                    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                    double beatsPerMeasure = static_cast<double>(timeSigNum);
                    double secondsPerMeasure = (beatsPerMeasure * 60.0) / bpm;
                    timelineState.virtualCursorTime = std::round(rawTime / secondsPerMeasure) * secondsPerMeasure;
                    
                    timelineState.showVirtualCursor = true;
                    timelineState.virtualCursorVisible = true;
                    timelineState.lastBlinkTime = std::chrono::steady_clock::now();
                    
                    break;
                }
            }
        }
    }
    
    wasLeftPressed = isLeftPressed;
}

// ==============================================
// STATE MANAGEMENT SUBSYSTEM IMPLEMENTATION (ADDITIONAL)
// ==============================================

void TimelineComponent::resetDragState() {
    dragState.isDraggingClip = false;
    dragState.isDraggingAudioClip = false;
    dragState.isDraggingMIDIClip = false;
    dragState.clipSelectedForDrag = false; // Clear the ready-to-drag state
    dragState.draggedAudioClip = nullptr;
    dragState.draggedMIDIClip = nullptr;
    dragState.dragStartMousePos = sf::Vector2f(0, 0);
    dragState.dragStartClipTime = 0.0;
    dragState.dragMouseOffsetInClip = 0.0;
    dragState.draggedTrackRowPos = sf::Vector2f(0, 0);
    dragState.draggedTrackName.clear();
}

void TimelineComponent::resetPlacementState() {
    placementState.isDraggingPlacement = false;
    placementState.isDraggingDeletion = false;
    placementState.currentSelectedTrack.clear();
    placementState.processedPositions.clear();
}

void TimelineComponent::clearProcessedPositions() {
    cacheState.cachedMeasureLines.clear();
    cacheState.lastMeasureWidth = -1.f;
    cacheState.lastScrollOffset = -1.f;
    cacheState.lastRowSize = sf::Vector2f(-1.f, -1.f);
}

void TimelineComponent::copySelectedClips() {
    clearClipboard();
    
    // Copy selected AudioClip
    if (selectedClip) {
        clipboardState.copiedAudioClips.push_back(*selectedClip);
        clipboardState.hasClipboard = true;
        DEBUG_PRINT("Copied AudioClip at time " + std::to_string(selectedClip->startTime));
    }
    
    // Copy selected MIDIClip
    MIDIClip* selectedMIDIClip = getSelectedMIDIClip();
    if (selectedMIDIClip) {
        clipboardState.copiedMIDIClips.push_back(*selectedMIDIClip);
        clipboardState.hasClipboard = true;
        DEBUG_PRINT("Copied MIDIClip at time " + std::to_string(selectedMIDIClip->startTime));
    }
    
    if (!clipboardState.hasClipboard) {
        DEBUG_PRINT("No clips selected to copy");
    }
}

void TimelineComponent::pasteClips() {
    if (!clipboardState.hasClipboard) {
        DEBUG_PRINT("No clips in clipboard to paste");
        return;
    }
    
    double cursorPosition = app->getPosition();
    std::string currentTrack = app->getSelectedTrack();
    
    if (currentTrack.empty()) {
        DEBUG_PRINT("No track selected for pasting");
        return;
    }
    
    // Paste AudioClips
    for (const AudioClip& originalClip : clipboardState.copiedAudioClips) {
        AudioClip newClip = originalClip;
        newClip.startTime = cursorPosition;
        app->addClipToTrack(currentTrack, newClip);
        DEBUG_PRINT("Pasted AudioClip at cursor position " + std::to_string(cursorPosition));
    }
    
    // Paste MIDIClips
    Track* track = app->getTrack(currentTrack);
    if (track && track->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        for (const MIDIClip& originalClip : clipboardState.copiedMIDIClips) {
            MIDIClip newClip = originalClip.createCopyAtTime(cursorPosition);
            midiTrack->addMIDIClip(newClip);
            DEBUG_PRINT("Pasted MIDIClip at cursor position " + std::to_string(cursorPosition));
        }
    }
}

void TimelineComponent::duplicateSelectedClips() {
    if (!selectedClip && !selectedMIDIClipInfo.hasSelection) {
        DEBUG_PRINT("No clips selected to duplicate");
        return;
    }
    
    std::string currentTrack = app->getSelectedTrack();
    if (currentTrack.empty()) {
        DEBUG_PRINT("No track selected for duplication");
        return;
    }
    
    // Duplicate AudioClip
    if (selectedClip) {
        AudioClip newClip = *selectedClip;
        newClip.startTime = selectedClip->startTime + selectedClip->duration;
        app->addClipToTrack(currentTrack, newClip);
        DEBUG_PRINT("Duplicated AudioClip, placed at time " + std::to_string(newClip.startTime));
    }
    
    // Duplicate MIDIClip
    MIDIClip* selectedMIDIClip = getSelectedMIDIClip();
    if (selectedMIDIClip) {
        Track* track = app->getTrack(currentTrack);
        if (track && track->getType() == Track::TrackType::MIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            double newStartTime = selectedMIDIClip->startTime + selectedMIDIClip->duration;
            MIDIClip newClip = selectedMIDIClip->createCopyAtTime(newStartTime);
            midiTrack->addMIDIClip(newClip);
            DEBUG_PRINT("Duplicated MIDIClip without gap, placed at time " + std::to_string(newClip.startTime));
        }
    }
}

void TimelineComponent::clearClipboard() {
    clipboardState.copiedAudioClips.clear();
    clipboardState.copiedMIDIClips.clear();
    clipboardState.hasClipboard = false;
}

// Static member definition
TimelineComponent* TimelineComponent::instance = nullptr;

// Plugin interface for TimelineComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(TimelineComponent)