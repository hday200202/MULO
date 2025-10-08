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
#include <algorithm>
#include <SFML/Window/Cursor.hpp>

class TimelineComponent : public MULOComponent {
public:
    static TimelineComponent* instance;
    static bool clipEndDrag;
    static bool clipStartDrag;
    static bool isResizing; // New flag to prevent interference during resize
    
    AudioClip* selectedClip = nullptr;
    double selectedClipEnd = 0.0;
    double originalClipStartTime = 0.0;
    double originalClipDuration = 0.0;
    double originalClipOffset = 0.0;
    double resizeDragStartMouseTime = 0.0;
    
    // MIDI clip resize state
    double originalMIDIClipStartTime = 0.0;
    double originalMIDIClipDuration = 0.0;
    bool midiClipEndDrag = false;
    bool midiClipStartDrag = false;
    bool isMIDIResizing = false;
    
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

    struct AutomationDragState {
        bool isDragging = false;
        sf::Vector2f startMousePos;
        float startTime = 0.0f;
        float startValue = 0.0f;
        std::string trackName;
        std::string effectName;
        std::string parameterName;
        std::string laneId;
    } automationDragState;

    struct UIElements {
        Row* masterTrackElement = nullptr;
        Button* muteMasterButton = nullptr;
        Slider* masterVolumeSlider = nullptr;
        Row* masterTrackLabel = nullptr;
        std::unordered_map<std::string, Button*> trackMuteButtons;
        std::unordered_map<std::string, Slider*> trackVolumeSliders;
        std::unordered_map<std::string, Button*> trackSoloButtons;
        std::unordered_map<std::string, Button*> trackRemoveButtons;
        std::unordered_map<std::string, TextBox*> trackNameTextBoxes;
        std::unordered_map<std::string, Text*> automationLaneLabels;
        std::unordered_map<std::string, Row*> automationLaneRows;
        std::unordered_map<std::string, Button*> automationClearButtons;
    } uiElements;

    void handleAllUserInput();
    void handleMouseInput();
    void handleKeyboardInput();
    void handleZoomChange(float newZoom);
    void handleClipInteractions();
    void handleTrackLeftClick();
    void handleTrackRightClick();
    
    void repositionAutomationLanes();
    void processDragOperations();
    void handleClipDragOperations();
    void handlePlacementDragOperations();
    void handleDeletionDragOperations();
    void handleAutomationDragOperations();
    void updateDragState();
    
    void updateTimelineVisuals();
    void updateScrolling();
    void updatePlayheadFollowing();
    void renderTrackContent();
    void updateVirtualCursor();
    
    void syncUIToEngine();
    void handleMasterTrackControls();
    
    void updateAutomationLaneLabels();
    void handleTrackControls();
    void handleIndividualTrackControls(Track* track, const std::string& name);
    void updateTrackHighlighting();
    void rebuildTrackUI();
    void handleTrackRenaming();
    
    void updateTimelineState();
    void resetDragState();
    void resetPlacementState();
    void clearProcessedPositions();

    Row* masterTrack();
    Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);    
    std::tuple<std::string, std::string, float> getCurrentAutomationParameter(Track* track);
    Row* automationLane(const std::string& trackName, const std::string& effectName, const std::string& parameterName, Align alignment, float currentValue = 0.5f, bool isPotential = false);

    void handleCustomUIElements();
    void handleDragOperations();
    void handleClipSelection();
    void handleTrackInteractions();
    void updateVisualState();
    
    void rebuildUIFromEngine();
    void syncSlidersToEngine();
    void processClipAtPosition(Track* track, const sf::Vector2f& localMousePos, bool isRightClick);
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedClipGeometry(const std::string& trackName, double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, UIResources* resources, UIState* uiState, const AudioClip* selectedClip, const std::string& currentTrackName, const std::string& selectedTrackName);
    
    // Clipboard functions
    void copySelectedClips();
    void pasteClips();
    void duplicateSelectedClips();
    void clearClipboard();
    
    // Cursor management
    void updateMouseCursor();
    void setCursor(sf::Cursor::Type cursorType);
    bool isMouseOverResizeZone(const AudioClip& clip, float mouseTimeInTrack) const;
    enum class ResizeZone { None, Start, End };
    ResizeZone getResizeZone(const AudioClip& clip, float mouseTimeInTrack, float pixelsPerSecond) const;
    ResizeZone getResizeZone(const MIDIClip& clip, float mouseTimeInTrack, float pixelsPerSecond) const;
    
    // Grid snapping helpers
    double snapToGrid(double timeValue, bool forceSnap = false) const;
    bool isShiftPressed() const;
    
    float enginePanToSlider(float enginePan) const { return (enginePan + 1.0f) * 0.5f; }
    float sliderPanToEngine(float sliderPan) const { return (sliderPan * 2.0f) - 1.0f; }
    
private:
    // Cursor state management
    sf::Cursor::Type currentCursorType = sf::Cursor::Type::Arrow;
    std::unique_ptr<sf::Cursor> resizeCursorH;
    std::unique_ptr<sf::Cursor> textCursor;
    std::unique_ptr<sf::Cursor> handCursor;
    bool cursorsEnabled = true; // Can be disabled if causing issues
    void initializeCursors();
};

inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, const std::vector<AudioClip>& clips, float verticalOffset, UIResources* resources, UIState* uiState, const AudioClip* selectedClip, const std::string& currentTrackName, const std::string& selectedTrackName);

inline std::vector<std::shared_ptr<sf::Drawable>> generateAutomationLine(
    const std::string& trackName,
    const std::string& effectName,
    const std::string& parameter,
    double bpm,
    float beatWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    float defaultValue,
    UIResources* resources,
    UIState* uiState,
    Application* app
);

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
inline double getSourceFileDuration(const AudioClip& clip);
inline void invalidateClipWaveform(const AudioClip& clip);

TimelineComponent::TimelineComponent() { 
    name = "timeline"; 
    selectedClip = nullptr;
    clipEndDrag = false;
    clipStartDrag = false;
    timelineState.lastFrameTime = std::chrono::steady_clock::now();
    timelineState.lastBlinkTime = std::chrono::steady_clock::now();
    timelineState.deltaTime = 0.0f;
    timelineState.firstFrame = true;
    
    instance = this;
    initializeCursors();
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
    
    static bool prevShowAutomation = app->readConfig("show_automation", false);
    bool currentShowAutomation = app->readConfig("show_automation", false);
    
    if (currentShowAutomation != prevShowAutomation) {
        prevShowAutomation = currentShowAutomation;
    }
    
    // Check if scrubber is being dragged to disable timeline mouse input
    bool scrubberDragging = app->readConfig<bool>("scrubber_dragging", false);
    features.enableMouseInput = !scrubberDragging;

    if (features.enableMouseInput || features.enableKeyboardInput) {
        handleAllUserInput();
    }
    
    updateTimelineVisuals();
    handleCustomUIElements();    
    handleTrackRenaming();
    
    // Update automation lane labels if automation view is enabled
    if (app->readConfig("show_automation", false)) {
        updateAutomationLaneLabels();
    }

    // Check if scrubber position has changed
    float scrubberPos = app->readConfig<float>("scrubber_position", 0.0f);
    scrubberPositionChanged = (std::abs(scrubberPos - lastScrubberPosition) > 0.001f);

    float mousePosSeconds = xPosToSeconds(
        app->getBpm(), 
        100.f * app->uiState.timelineZoomLevel, 
        app->ui->getMousePosition().x, 
        timelineState.timelineOffset
    );
    
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
    // Only handle events when timeline is visible
    if (!this->isVisible()) {
        timelineState.wasVisible = false;
        return false;
    }
    
    bool forceUpdate = app->isPlaying();

    if (!timelineState.wasVisible) {
        syncSlidersToEngine();
        timelineState.wasVisible = true;
    }

    if (features.enableUISync) {
        syncUIToEngine();
    }

    if (app->freshRebuild) {
        rebuildUI();
    }
    
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
        0.75f,
        "Master_volume_slider"
    );

    uiElements.masterTrackLabel = row(
        Modifier(),
    contains{
        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        column(
            Modifier(),
        contains{
            text(
                Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                " Master",
                app->resources.dejavuSansFont,
                "Master_name_text"
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
        0.75f,
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
            if (trackRow && !isResizing) { // Don't update virtual cursor during resize
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
                    
                    [&]() {
                        auto* trackNameTextBox = textBox(
                            Modifier().setColor(sf::Color::Transparent).setfixedHeight(28).align(Align::LEFT | Align::TOP),
                            TBStyle::Default,
                            app->resources.dejavuSansFont,
                            "", // Empty default text
                            app->resources.activeTheme->primary_text_color,
                            sf::Color::Transparent,
                            trackName + "_name_textbox"
                        );
                        
                        // Set the actual text content to the track name
                        trackNameTextBox->setText(trackName);
                        
                        // Store reference for rename handling
                        uiElements.trackNameTextBoxes[trackName] = trackNameTextBox;
                        
                        return trackNameTextBox;
                    }()
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

std::tuple<std::string, std::string, float> TimelineComponent::getCurrentAutomationParameter(Track* track) {
    if (!track) {
        return {"", "Volume", track ? track->getVolume() : 1.0f};
    }
    
    // First check if track has stored potential automation
    if (track->hasPotentialAutomation()) {
        const auto& potentialAuto = track->getPotentialAutomation();
        if (!potentialAuto.first.empty() && !potentialAuto.second.empty()) {
            // Check if this is a built-in track parameter
            if (potentialAuto.first == "Track") {
                float currentValue = track->getCurrentParameterValue(potentialAuto.first, potentialAuto.second);
                return {potentialAuto.first, potentialAuto.second, currentValue};
            }
            
            // Try to get the current value of this effect parameter
            auto& effects = track->getEffects();
            for (size_t i = 0; i < effects.size(); ++i) {
                const auto& effect = effects[i];
                if (!effect) continue;
                
                std::string effectKey = effect->getName() + "_" + std::to_string(i);
                if (effectKey == potentialAuto.first) {
                    auto params = effect->getAllParameters();
                    for (int p = 0; p < params.size(); ++p) {
                        auto* param = params[p];
                        if (!param) continue;
                        
                        std::string paramName = param->getName(256).toStdString();
                        if (paramName == potentialAuto.second) {
                            float currentValue = param->getValue();
                            return {effectKey, paramName, currentValue};
                        }
                    }
                }
            }
        }
    }
    
    // If no potential automation, check if track has any effects at all
    auto& effects = track->getEffects();
    if (!effects.empty()) {
        // Find the first effect with parameters and use its first parameter
        for (size_t i = 0; i < effects.size(); ++i) {
            const auto& effect = effects[i];
            if (!effect) continue;
            
            auto params = effect->getAllParameters();
            if (!params.isEmpty()) {
                auto* param = params[0];
                if (param) {
                    std::string effectKey = effect->getName() + "_" + std::to_string(i);
                    std::string paramName = param->getName(256).toStdString();
                    float currentValue = param->getValue();
                    return {effectKey, paramName, currentValue};
                }
            }
        }
    }
    
    // Fallback to Volume with proper normalization
    float normalizedVolume = track->getCurrentParameterValue("Track", "Volume");
    return {"Track", "Volume", normalizedVolume};
}

uilo::Row* TimelineComponent::automationLane(
    const std::string& trackName,
    const std::string& effectName,
    const std::string& parameterName,
    Align alignment,
    float currentValue,
    bool isPotential
) {
    std::string laneId = isPotential ? 
        (trackName + "_potential") : 
        (trackName + "_" + effectName + "_" + parameterName);
    std::string displayName = effectName.empty() ? parameterName : effectName + " - " + parameterName;
    
    // Create scrollable row element like in the track function
    auto* scrollableRowElement = scrollableRow(
        Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains {}, laneId + "_scrollable_row"
    );
    containers[laneId + "_scrollable_row"] = scrollableRowElement;

    auto handleAutomationClick = [this, trackName, laneId]() {
        if (!app->getWindow().hasFocus()) return;
        
        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        auto* automationRow = containers[laneId + "_scrollable_row"];
        if (automationRow) {
            sf::Vector2f localMousePos = globalMousePos - automationRow->getPosition();            
            float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineState.timelineOffset, timelineState.timelineOffset);
            
            sf::Vector2f rowSize = automationRow->getSize();
            float laneHeight = rowSize.y;
            float automationValue = 1.0f - (localMousePos.y / laneHeight);
            automationValue = std::max(0.0f, std::min(1.0f, automationValue));
            
            if (timePosition >= 0.0f) {
                Track* track = app->getTrack(trackName);
                if (track) {
                    std::string currentEffectName;
                    std::string currentParameterName;
                    
                    // Determine if this is a potential automation lane
                    bool isPotentialLane = (laneId.find("_potential") != std::string::npos);
                    
                    if (isPotentialLane && track->hasPotentialAutomation()) {
                        const auto& potentialAuto = track->getPotentialAutomation();
                        currentEffectName = potentialAuto.first;
                        currentParameterName = potentialAuto.second;
                    } else {
                        // Extract from laneId format: trackName_effectName_parameterName
                        std::string prefix = trackName + "_";
                        if (laneId.size() > prefix.size()) {
                            std::string paramPart = laneId.substr(prefix.size());
                            size_t lastUnderscore = paramPart.find_last_of('_');
                            if (lastUnderscore != std::string::npos) {
                                currentEffectName = paramPart.substr(0, lastUnderscore);
                                currentParameterName = paramPart.substr(lastUnderscore + 1);
                            } else {
                                currentEffectName = "";
                                currentParameterName = paramPart;
                            }
                        }
                    }
                    
                    if (!currentParameterName.empty()) {
                        // Check if clicking near existing automation point (for dragging)
                        const auto* points = track->getAutomationPoints(currentEffectName, currentParameterName);
                        bool startedDrag = false;
                        
                        if (points) {
                            constexpr float clickTolerance = 10.0f; // pixels
                            auto valueToY = [rowSize](float value) { return rowSize.y * (1.0f - value); };
                            auto timeToX = [this](float time) { 
                                return time * 100.f * app->uiState.timelineZoomLevel + timelineState.timelineOffset; 
                            };
                            
                            for (const auto& point : *points) {
                                float pointX = timeToX(point.time);
                                float pointY = valueToY(point.value);
                                
                                float distance = std::sqrt(std::pow(localMousePos.x - pointX, 2) + std::pow(localMousePos.y - pointY, 2));
                                
                                if (distance <= clickTolerance) {
                                    // Start dragging this point
                                    automationDragState.isDragging = true;
                                    automationDragState.startMousePos = globalMousePos;
                                    automationDragState.startTime = point.time;
                                    automationDragState.startValue = point.value;
                                    automationDragState.trackName = trackName;
                                    automationDragState.effectName = currentEffectName;
                                    automationDragState.parameterName = currentParameterName;
                                    automationDragState.laneId = laneId;
                                    startedDrag = true;
                                    break;
                                }
                            }
                        }
                        
                        // If not dragging, add new automation point
                        if (!startedDrag) {
                            // Check if Shift is held to bypass snapping
                            bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                            
                            if (!shiftHeld) {
                                auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                                auto measureLines = generateTimelineMeasures(100.f * app->uiState.timelineZoomLevel, timelineState.timelineOffset, rowSize, timeSigNum, timeSigDen, &app->resources);
                                sf::Vector2f snapPos(localMousePos.x, localMousePos.y);
                                float snapX = getNearestMeasureX(snapPos, measureLines);
                                timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, snapX - timelineState.timelineOffset, timelineState.timelineOffset);
                            }
                            
                            Track::AutomationPoint point;
                            point.time = timePosition;
                            point.value = automationValue;
                            point.curve = 0.0f;
                            
                            // Check if there are already 2 points at this time position (allow small tolerance)
                            const auto* existingPoints = track->getAutomationPoints(currentEffectName, currentParameterName);
                            int pointsAtThisTime = 0;
                            constexpr float timeTolerance = 0.001f;
                            
                            if (existingPoints) {
                                for (const auto& existingPoint : *existingPoints) {
                                    if (std::abs(existingPoint.time - timePosition) < timeTolerance) {
                                        pointsAtThisTime++;
                                    }
                                }
                            }
                            
                            // Only add point if there are fewer than 2 points at this time
                            if (pointsAtThisTime < 2) {
                                track->addAutomationPoint(currentEffectName, currentParameterName, point);
                            }
                            // If there are already 2 points, silently ignore the add request
                        }
                    }
                }
            }
        }
    };

    auto handleAutomationRightClick = [this, trackName, laneId]() {
        if (!app->getWindow().hasFocus()) return;
        
        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        auto* automationRow = containers[laneId + "_scrollable_row"];
        if (automationRow) {
            sf::Vector2f localMousePos = globalMousePos - automationRow->getPosition();
            sf::Vector2f rowSize = automationRow->getSize();
            
            Track* track = app->getTrack(trackName);
            if (track) {
                std::string currentEffectName;
                std::string currentParameterName;
                
                // Determine if this is a potential automation lane
                bool isPotentialLane = (laneId.find("_potential") != std::string::npos);
                
                if (isPotentialLane && track->hasPotentialAutomation()) {
                    const auto& potentialAuto = track->getPotentialAutomation();
                    currentEffectName = potentialAuto.first;
                    currentParameterName = potentialAuto.second;
                } else {
                    // Extract from laneId format: trackName_effectName_parameterName
                    std::string prefix = trackName + "_";
                    if (laneId.size() > prefix.size()) {
                        std::string paramPart = laneId.substr(prefix.size());
                        size_t lastUnderscore = paramPart.find_last_of('_');
                        if (lastUnderscore != std::string::npos) {
                            currentEffectName = paramPart.substr(0, lastUnderscore);
                            currentParameterName = paramPart.substr(lastUnderscore + 1);
                        } else {
                            currentEffectName = "";
                            currentParameterName = paramPart;
                        }
                    }
                }
                
                if (!currentParameterName.empty()) {
                    // Find the closest automation point to the click position (both X and Y)
                    const auto* points = track->getAutomationPoints(currentEffectName, currentParameterName);
                    if (points) {
                        constexpr float xTolerance = 30.0f; // pixels in X direction
                        constexpr float yTolerance = 20.0f; // pixels in Y direction
                        
                        // Use the same coordinate calculations as the rendering system
                        auto valueToY = [rowSize](float value) { 
                            const float padding = 4.0f;
                            const float usableHeight = rowSize.y - 2 * padding;
                            float normalizedValue = std::max(0.0f, std::min(value, 1.0f));
                            return padding + (1.0f - normalizedValue) * usableHeight;
                        };
                        auto timeToX = [this](float time) { 
                            return secondsToXPosition(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, time) + timelineState.timelineOffset;
                        };
                        
                        float closestDistance = std::numeric_limits<float>::max();
                        float targetTime = -1.0f;
                        
                        for (const auto& point : *points) {
                            float pointX = timeToX(point.time);
                            float pointY = valueToY(point.value);
                            
                            float xDiff = std::abs(localMousePos.x - pointX);
                            float yDiff = std::abs(localMousePos.y - pointY);
                            
                            // Check if within tolerances
                            if (xDiff <= xTolerance && yDiff <= yTolerance) {
                                // Use combined distance for closest point selection
                                float distance = std::sqrt(xDiff * xDiff + yDiff * yDiff);
                                if (distance < closestDistance) {
                                    closestDistance = distance;
                                    targetTime = point.time;
                                }
                            }
                        }
                        
                        // Remove the closest point if found
                        if (targetTime >= 0.0f) {
                            track->removeAutomationPoint(currentEffectName, currentParameterName, targetTime, 0.001f);
                        }
                    }
                }
            }
        }
    };
    
    scrollableRowElement->m_modifier.onLClick([=]() { handleAutomationClick(); });
    scrollableRowElement->m_modifier.onRClick([=]() { handleAutomationRightClick(); });

    auto* automationLabelColumn = column(
        Modifier()
            .align(Align::RIGHT)
            .setfixedWidth(196)
            .setColor(app->resources.activeTheme->not_muted_color),
    contains{
        spacer(Modifier().setfixedHeight(8).align(Align::TOP)),

        [&]() -> Element* {
            if (isPotential) {
                // For potential automation lanes, skip the button column entirely and start text from left
                return row(
                    Modifier().align(Align::LEFT | Align::CENTER_Y).setHighPriority(true),
                contains{
                    spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
                    
                    [&]() {
                        auto* textElement = text(
                            Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16).align(Align::LEFT | Align::CENTER_Y),
                            "[+] " + displayName,
                            app->resources.dejavuSansFont,
                            laneId + "_label_text"
                        );
                        
                        // Store reference for dynamic updates
                        std::string updateKey = laneId;
                        uiElements.automationLaneLabels[updateKey] = textElement;
                        
                        return textElement;
                    }()
                });
            } else {
                // For regular automation lanes, use the button + text layout
                return row(
                    Modifier().align(Align::RIGHT).setHighPriority(true),
                contains{
                    // Red clear button column
                    [&]() {
                        auto* clearButton = button(
                            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedWidth(16).setfixedHeight(16).setColor(app->resources.activeTheme->mute_color)
                                .onLClick([this, trackName, effectName, parameterName](){
                                    if (!app->getWindow().hasFocus()) return;
                                    Track* track = app->getTrack(trackName);
                                    if (track) {
                                        track->clearAutomationParameter(effectName, parameterName);
                                        std::cout << "[AUTOMATION] Cleared automation for " << effectName << " - " << parameterName << std::endl;
                                    }
                                }),
                            ButtonStyle::Pill,
                            "",
                            "",
                            sf::Color::Transparent,
                            "clear_" + laneId
                        );
                        
                        // Store reference for click handling
                        std::string buttonKey = trackName + "_" + effectName + "_" + parameterName + "_clear";
                        uiElements.automationClearButtons[buttonKey] = clearButton;
                        
                        return column(
                            Modifier().setfixedWidth(32).align(Align::LEFT | Align::TOP),
                        contains{
                            clearButton
                        });
                    }(),
                    
                    // Parameter name column
                    column(
                        Modifier(),
                    contains{
                        row(
                            Modifier().align(Align::LEFT | Align::CENTER_Y),
                        contains{
                            spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
                            
                            [&]() {
                                auto* textElement = text(
                                    Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16).align(Align::LEFT | Align::CENTER_Y),
                                    displayName,
                                    app->resources.dejavuSansFont,
                                    laneId + "_label_text"
                                );
                                
                                // Store reference for dynamic updates
                                std::string updateKey = laneId;
                                uiElements.automationLaneLabels[updateKey] = textElement;
                                
                                return textElement;
                            }()
                        })
                    })
                });
            }
        }(),

        spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
    }, laneId + "_label");

    containers[laneId + "_label"] = automationLabelColumn;

    auto* automationRow = row(
        Modifier()
            .setColor(app->resources.activeTheme->not_muted_color)  // Darker than track_row_color
            .setfixedHeight(96)  // Same height as track rows
            .align(alignment),
    contains{
        scrollableRowElement,
        automationLabelColumn
    }, laneId + "_automation_row");
    
    // Store the row for updates
    std::string updateKey = trackName + "_automation";
    uiElements.automationLaneRows[updateKey] = automationRow;
    
    return automationRow;
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
                            if (!dragState.isDraggingClip && !dragState.clipSelectedForDrag && !isResizing && !isMIDIResizing) {
                                float midiMouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                
                                ResizeZone zone = getResizeZone(mc, midiMouseTimeInTrack, pixelsPerSecond);
                                
                                selectedMIDIClipInfo.hasSelection = true;
                                selectedMIDIClipInfo.startTime = mc.startTime;
                                selectedMIDIClipInfo.duration = mc.duration;
                                selectedMIDIClipInfo.trackName = track->getName();
                                selectedClip = nullptr;
                                app->setSelectedTrack(track->getName());
                                
                                if (zone == ResizeZone::End) {
                                    midiClipEndDrag = true;
                                    midiClipStartDrag = false;
                                    if (!isMIDIResizing) {
                                        originalMIDIClipStartTime = mc.startTime;
                                        originalMIDIClipDuration = mc.duration;
                                        resizeDragStartMouseTime = midiMouseTimeInTrack;
                                    }
                                    isMIDIResizing = true;
                                    
                                    timelineState.virtualCursorTime = mc.startTime + mc.duration;
                                    timelineState.showVirtualCursor = true;
                                    timelineState.virtualCursorVisible = true;
                                } else if (zone == ResizeZone::Start) {
                                    midiClipStartDrag = true;
                                    midiClipEndDrag = false;
                                    if (!isMIDIResizing) {
                                        originalMIDIClipStartTime = mc.startTime;
                                        originalMIDIClipDuration = mc.duration;
                                        resizeDragStartMouseTime = midiMouseTimeInTrack;
                                    }
                                    isMIDIResizing = true;
                                    
                                    timelineState.virtualCursorTime = mc.startTime;
                                    timelineState.showVirtualCursor = true;
                                    timelineState.virtualCursorVisible = true;
                                } else {
                                    midiClipEndDrag = false;
                                    midiClipStartDrag = false;
                                    isMIDIResizing = false;
                                }
                                
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
                            if (!dragState.isDraggingClip && !dragState.clipSelectedForDrag && !isResizing) {
                                // Calculate mouse position in seconds
                                float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                
                                ResizeZone zone = getResizeZone(ac, mouseTimeInTrack, pixelsPerSecond);
                                
                                selectedClip = const_cast<AudioClip*>(&clipsVec[i]);
                                selectedClipEnd = selectedClip->startTime + selectedClip->duration;
                                selectedMIDIClipInfo.hasSelection = false;
                                app->setSelectedTrack(track->getName());
                                
                                if (zone == ResizeZone::End) {
                                    clipEndDrag = true;
                                    clipStartDrag = false;
                                    if (!isResizing) {
                                        originalClipDuration = selectedClip->duration;
                                        resizeDragStartMouseTime = mouseTimeInTrack;
                                    }
                                    isResizing = true;
                                    
                                    timelineState.virtualCursorTime = selectedClip->startTime + selectedClip->duration;
                                    timelineState.showVirtualCursor = true;
                                    timelineState.virtualCursorVisible = true;
                                } else if (zone == ResizeZone::Start) {
                                    clipStartDrag = true;
                                    clipEndDrag = false;
                                    if (!isResizing) {
                                        originalClipStartTime = selectedClip->startTime;
                                        originalClipDuration = selectedClip->duration;
                                        originalClipOffset = selectedClip->offset;
                                        resizeDragStartMouseTime = mouseTimeInTrack;
                                    }
                                    isResizing = true;
                                    
                                    timelineState.virtualCursorTime = selectedClip->startTime;
                                    timelineState.showVirtualCursor = true;
                                    timelineState.virtualCursorVisible = true;
                                } else {
                                    clipEndDrag = false;
                                    clipStartDrag = false;
                                    isResizing = false;
                                }
                                
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
                                
                                // Only enable clip dragging if we're not resizing
                                if (!clipEndDrag && !clipStartDrag) {
                                    dragState.clipSelectedForDrag = true;
                                    dragState.draggedAudioClip = selectedClip;
                                    dragState.draggedMIDIClip = nullptr;
                                    dragState.dragStartMousePos = mousePos;
                                    dragState.dragStartClipTime = ac.startTime;
                                    
                                    // Calculate where within the clip the mouse initially clicked
                                    float audioMouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                                    dragState.dragMouseOffsetInClip = audioMouseTimeInTrack - ac.startTime;
                                } else {
                                    // Skipping normal drag because resize mode is active
                                }
                                
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
                
                if (!clickedOnClip && localMousePos.x >= 0 && localMousePos.y >= 0 && localMousePos.y <= trackRow->getSize().y && !isResizing && !isMIDIResizing) { // Don't update virtual cursor during resize
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
                    // Cleared clip selection (clicked empty area)
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
    
    bool showAutomation = app->readConfig("show_automation", false);
    
    if (showAutomation) {
        const auto& allTracks = app->getAllTracks();
        
        for (const auto& track : allTracks) {
            if (track->getName() == "Master") continue;
            
            // Get current automation state from Track
            const auto& automatedParams = track->getAutomatedParameters();
            bool hasPotentialAutomation = track->hasPotentialAutomation();
            std::pair<std::string, std::string> potentialAuto;
            if (hasPotentialAutomation) {
                potentialAuto = track->getPotentialAutomation();
            }
            
            // Track which lanes should exist
            std::set<std::string> expectedLaneIds;
            
            // Add IDs for automated parameter lanes (scrollable_row format)
            for (const auto& [effectName, parameterName] : automatedParams) {
                std::string laneId = track->getName() + "_" + effectName + "_" + parameterName + "_automation_scrollable_row";
                expectedLaneIds.insert(laneId);
            }
            
            // Add ID for potential automation lane (scrollable_row format)
            if (hasPotentialAutomation) {
                std::string potentialLaneId = track->getName() + "_potential_automation_scrollable_row";
                expectedLaneIds.insert(potentialLaneId);
            }
            
            // Find existing automation lanes for this track  
            const auto& elements = timelineElement->getElements();
            std::set<std::string> existingLaneIds;
            std::set<std::string> existingAutomationRows;
            for (auto* element : elements) {
                // Look for automation rows with exact track name matching
                if (element->m_name.find("_automation_row") != std::string::npos) {
                    // Extract the track name from the element name
                    // Format should be: trackName_effectName_parameterName_automation_row
                    //             or: trackName_potential_automation_row
                    
                    std::string elementName = element->m_name;
                    std::string trackName = track->getName();
                    
                    // Check if this element belongs to the current track
                    bool belongsToThisTrack = false;
                    
                    if (elementName.find("_potential_automation_row") != std::string::npos) {
                        // For potential automation: trackName_potential_automation_row
                        std::string expectedPotentialName = trackName + "_potential_automation_row";
                        if (elementName == expectedPotentialName) {
                            belongsToThisTrack = true;
                        }
                    } else {
                        // For regular automation: trackName_effectName_parameterName_automation_row
                        // Check if it starts with trackName_ and the next part is NOT another track name
                        std::string expectedPrefix = trackName + "_";
                        if (elementName.find(expectedPrefix) == 0) {
                            // Get the part after trackName_
                            std::string afterTrackName = elementName.substr(expectedPrefix.length());
                            // Make sure this isn't another track (like drums_1 when track is drums)
                            // Check if the character after track name is a digit (indicating it's another track)
                            if (!afterTrackName.empty() && !std::isdigit(afterTrackName[0])) {
                                belongsToThisTrack = true;
                            }
                        }
                    }
                    
                    if (belongsToThisTrack) {
                        existingAutomationRows.insert(element->m_name);
                    }
                }
            }
            
            // Convert automation row names to expected scrollable row names for comparison
            for (const auto& rowId : existingAutomationRows) {
                if (rowId.find("_potential_automation_row") != std::string::npos) {
                    std::string converted = track->getName() + "_potential_automation_scrollable_row";
                    existingLaneIds.insert(converted);
                } else if (rowId.find("_automation_row") != std::string::npos) {
                    // Convert from "_automation_row" suffix to "_automation_scrollable_row"
                    std::string scrollableId = rowId;
                    // Replace "_automation_row" with "_automation_scrollable_row"
                    size_t pos = scrollableId.find("_automation_row");
                    if (pos != std::string::npos) {
                        scrollableId.replace(pos, 15, "_automation_scrollable_row");
                        existingLaneIds.insert(scrollableId);
                    }
                }
            }
            
            if (expectedLaneIds != existingLaneIds) {
                // Find track row position for insertion
                std::string trackRowId = track->getName() + "_track_row";
                int insertIndex = -1;
                const auto& currentElements = timelineElement->getElements();
                for (int i = 0; i < static_cast<int>(currentElements.size()); ++i) {
                    if (currentElements[i]->m_name == trackRowId) {
                        insertIndex = i + 1;
                        break;
                    }
                }
                
                // Remove lanes that shouldn't exist anymore
                for (const auto& existingScrollableId : existingLaneIds) {
                    if (expectedLaneIds.find(existingScrollableId) == expectedLaneIds.end()) {
                        // Convert scrollable_row ID back to automation_row ID to find the actual element
                        std::string automationRowId;
                        if (existingScrollableId.find("_potential_automation_scrollable_row") != std::string::npos) {
                            automationRowId = track->getName() + "_potential_automation_row";
                        } else if (existingScrollableId.find("_automation_scrollable_row") != std::string::npos) {
                            // Convert from automation_scrollable_row to automation_row
                            automationRowId = existingScrollableId;
                            size_t pos = automationRowId.find("_automation_scrollable_row");
                            if (pos != std::string::npos) {
                                automationRowId.replace(pos, 26, "_automation_row");
                            }
                        }
                        
                        // Find and remove the automation row element and its preceding spacer
                        const auto& elements = timelineElement->getElements();
                        for (int i = 0; i < static_cast<int>(elements.size()); ++i) {
                            if (elements[i]->m_name == automationRowId) {
                                // Remove the automation lane
                                timelineElement->removeElement(elements[i]);

                                if (i > 0) {
                                    auto* prevElement = elements[i-1];
                                    // Check if the previous element is a named automation spacer or generic spacer
                                    if (prevElement->m_name.empty() || 
                                        prevElement->m_name.find("spacer") != std::string::npos ||
                                        prevElement->m_name.find("_automation_spacer") != std::string::npos ||
                                        prevElement->m_name.find("_potential_automation_spacer") != std::string::npos) {
                                        timelineElement->removeElement(prevElement);
                                    }
                                }
                                break;
                            }
                        }
                        
                        containers.erase(existingScrollableId);
                    }
                }
                
                // Clean up any orphaned spacers that belong to this track
                const auto& cleanupElements = timelineElement->getElements();
                std::vector<Element*> spacersToRemove;
                for (auto* element : cleanupElements) {
                    if (element->m_name == track->getName() + "_automation_spacer" ||
                        element->m_name == track->getName() + "_potential_automation_spacer") {
                        spacersToRemove.push_back(element);
                    }
                }
                for (auto* spacer : spacersToRemove) {
                    timelineElement->removeElement(spacer);
                }
                
                // Add new lanes that should exist but don't
                if (insertIndex >= 0) {
                    for (const auto& [effectName, parameterName] : automatedParams) {
                        std::string laneScrollableId = track->getName() + "_" + effectName + "_" + parameterName + "_automation_scrollable_row";
                        if (existingLaneIds.find(laneScrollableId) == existingLaneIds.end()) {
                            float currentValue = track->getCurrentParameterValue(effectName, parameterName);
                            auto* automationRowElem = automationLane(
                                track->getName(), 
                                effectName,
                                parameterName,
                                Align::TOP | Align::LEFT, 
                                currentValue,
                                false
                            );
                            
                            timelineElement->insertElementAt(automationRowElem, insertIndex);
                            insertIndex += 1;

                        }
                    }
                    
                    // Add potential automation lane if needed
                    if (hasPotentialAutomation) {
                        std::string potentialScrollableId = track->getName() + "_potential_automation_scrollable_row";
                        if (existingLaneIds.find(potentialScrollableId) == existingLaneIds.end()) {
                            float currentValue = track->getCurrentParameterValue(potentialAuto.first, potentialAuto.second);
                            auto* potentialRowElem = automationLane(
                                track->getName(), 
                                potentialAuto.first,
                                potentialAuto.second,
                                Align::TOP | Align::LEFT, 
                                currentValue,
                                true
                            );
                            
                            timelineElement->insertElementAt(potentialRowElem, insertIndex);
                        }
                    }
                }
            }
        }
        
        // Automation lane geometry
        for (const auto& track : allTracks) {
            if (track->getName() == "Master") continue;
            
            const double beatWidth = 100.f * app->uiState.timelineZoomLevel;
            auto [timeSigNum, timeSigDen] = app->getTimeSignature();
            
            const auto& automatedParams = track->getAutomatedParameters();
            for (const auto& [effectName, parameterName] : automatedParams) {
                std::string laneId = track->getName() + "_" + effectName + "_" + parameterName + "_scrollable_row";
                auto laneIt = containers.find(laneId);
                if (laneIt != containers.end() && laneIt->second) {
                    auto* automationRow = static_cast<uilo::ScrollableRow*>(laneIt->second);
                    auto lines = generateTimelineMeasures(beatWidth, clampedOffset, automationRow->getSize(), timeSigNum, timeSigDen, &app->resources);
                    float currentValue = track->getCurrentParameterValue(effectName, parameterName);
                    
                    // Generate automation line visualization
                    auto automationLineDrawables = generateAutomationLine(
                        track->getName(),
                        effectName,
                        parameterName,
                        app->getBpm(),
                        static_cast<float>(beatWidth),
                        static_cast<float>(clampedOffset),
                        automationRow->getSize(),
                        currentValue,
                        &app->resources,
                        &app->uiState,
                        app
                    );
                    
                    std::vector<std::shared_ptr<sf::Drawable>> automationGeometry;
                    automationGeometry.reserve(lines.size() + automationLineDrawables.size());                    
                    automationGeometry.insert(automationGeometry.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
                    automationGeometry.insert(automationGeometry.end(), std::make_move_iterator(automationLineDrawables.begin()), std::make_move_iterator(automationLineDrawables.end()));
                    automationRow->setCustomGeometry(automationGeometry);
                }
            }
            
            // Populate potential automation lane
            if (track->hasPotentialAutomation()) {
                std::string potentialLaneId = track->getName() + "_potential_scrollable_row";
                auto potentialIt = containers.find(potentialLaneId);
                if (potentialIt != containers.end() && potentialIt->second) {
                    auto* automationRow = static_cast<uilo::ScrollableRow*>(potentialIt->second);
                    auto lines = generateTimelineMeasures(beatWidth, clampedOffset, automationRow->getSize(), timeSigNum, timeSigDen, &app->resources);
                    const auto& potentialAuto = track->getPotentialAutomation();
                    float currentValue = track->getCurrentParameterValue(potentialAuto.first, potentialAuto.second);
                    
                    auto automationLineDrawables = generateAutomationLine(
                        track->getName(),
                        potentialAuto.first,
                        potentialAuto.second,
                        app->getBpm(),
                        static_cast<float>(beatWidth),
                        static_cast<float>(clampedOffset),
                        automationRow->getSize(),
                        currentValue,
                        &app->resources,
                        &app->uiState,
                        app
                    );
                    
                    std::vector<std::shared_ptr<sf::Drawable>> automationGeometry;
                    automationGeometry.reserve(lines.size() + automationLineDrawables.size());                    
                    automationGeometry.insert(automationGeometry.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
                    automationGeometry.insert(automationGeometry.end(), std::make_move_iterator(automationLineDrawables.begin()), std::make_move_iterator(automationLineDrawables.end()));
                    automationRow->setCustomGeometry(automationGeometry);
                }
            }
        }
    } else {
        // Remove all automation lanes when automation view is disabled
        const auto& allTracks = app->getAllTracks();
        for (const auto& track : allTracks) {
            if (track->getName() == "Master") continue;
            
            const auto& elements = timelineElement->getElements();
            
            std::vector<uilo::Element*> toRemove;
            for (auto* element : elements) {
                if (element->m_name.find(track->getName() + "_") != std::string::npos &&
                    element->m_name.find("_automation_row") != std::string::npos) {
                    toRemove.push_back(element);
                }
            }
            
            for (auto* element : toRemove) {
                timelineElement->removeElement(element);
            }
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
            
            if (dragDistance > dragState.DRAG_THRESHOLD && !clipEndDrag && !clipStartDrag && !midiClipEndDrag && !midiClipStartDrag) {
                dragState.isDraggingClip = true;
                dragState.isDraggingAudioClip = (dragState.draggedAudioClip != nullptr);
                dragState.isDraggingMIDIClip = (dragState.draggedMIDIClip != nullptr);
                dragState.clipSelectedForDrag = false;
            } else if (clipEndDrag || clipStartDrag || midiClipEndDrag || midiClipStartDrag) {
                // Preventing normal drag because resize mode is active
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
                if (clipEndDrag || clipStartDrag) {
                    // Skipping audio clip position update because resize is active
                } else {
                    dragState.draggedAudioClip->startTime = newStartTime;
                }
            } else if (dragState.isDraggingMIDIClip && dragState.draggedMIDIClip) {
                if (clipEndDrag || clipStartDrag) {
                    // Skipping MIDI clip position update because resize is active
                } else {
                    dragState.draggedMIDIClip->startTime = newStartTime;
                }
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

void TimelineComponent::repositionAutomationLanes() {    
    // Find the timeline scrollable column
    uilo::ScrollableColumn* timelineScrollable = nullptr;
    
    auto timelineScrollableIt = containers.find("timeline_scrollable");
    if (timelineScrollableIt != containers.end()) {
        timelineScrollable = static_cast<uilo::ScrollableColumn*>(timelineScrollableIt->second);
    } else {
        auto timelineIt = containers.find("timeline");
        if (timelineIt != containers.end()) {
            timelineScrollable = static_cast<uilo::ScrollableColumn*>(timelineIt->second);
        }
    }
    
    if (!timelineScrollable) {
        return;
    }
    
    // Get all automation lanes and their track names
    std::vector<std::pair<std::string, uilo::Element*>> automationLanes;
    
    const auto& elements = timelineScrollable->getElements();
    for (auto* element : elements) {
        std::string elementName = element->m_name;
        if (elementName.find("_automation_row") != std::string::npos) {
            size_t firstUnderscore = elementName.find('_');
            if (firstUnderscore != std::string::npos) {
                std::string trackName = elementName.substr(0, firstUnderscore);
                automationLanes.push_back({trackName, element});
            }
        }
    }
    
    // For each automation lane, move it to the correct position after its track
    for (auto& [trackName, lane] : automationLanes) {        
        // Find the track element directly in the timeline by name
        int trackIndex = -1;
        const auto& currentElements = timelineScrollable->getElements();
        for (int i = 0; i < static_cast<int>(currentElements.size()); ++i) {
            std::string elementName = currentElements[i]->m_name;
            if (elementName == trackName + "_scrollable_row") {
                trackIndex = i;
                break;
            }
        }
        
        if (trackIndex >= 0) {
            // Find the correct position after this track's automation lanes
            int targetIndex = trackIndex + 1;
            
            // Count existing automation lanes for this track that are already positioned correctly
            for (int i = trackIndex + 1; i < static_cast<int>(currentElements.size()); ++i) {
                if (currentElements[i] == lane) break; // Don't count the lane we're moving
                
                std::string elementName = currentElements[i]->m_name;
                if (elementName.find(trackName + "_") == 0 && 
                    elementName.find("_automation_row") != std::string::npos) {
                    targetIndex = i + 1;
                } else if (elementName.find("_scrollable_row") != std::string::npos && 
                    elementName.find("_automation_row") == std::string::npos) {
                    break;
                }
            }
            
            // Move the automation lane to the correct position
            int currentIndex = timelineScrollable->getElementIndex(lane);
            
            if (currentIndex != targetIndex) {
                timelineScrollable->insertElementAt(lane, targetIndex);
            }
        }
    }
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
    // Clear waveform cache
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

inline double getSourceFileDuration(const AudioClip& clip) {
    if (!clip.sourceFile.existsAsFile()) return 0.0;
    
    static thread_local juce::AudioFormatManager formatManager;
    static thread_local bool initialized = false;
    if (!initialized) {
        formatManager.registerBasicFormats();
        initialized = true;
    }
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(clip.sourceFile));
    if (!reader) return 0.0;
    
    return reader->lengthInSamples / reader->sampleRate;
}

inline void invalidateClipWaveform(const AudioClip& clip) {
    if (!clip.sourceFile.existsAsFile()) return;
    
    auto& cache = getWaveformCache();
    const std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    
    auto it = cache.find(filePath);
    if (it != cache.end()) {
        cache.erase(it);
        // Invalidated cache for file
    }
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

inline std::vector<std::shared_ptr<sf::Drawable>> generateAutomationLine(
    const std::string& trackName,
    const std::string& effectName,
    const std::string& parameter,
    double bpm,
    float beatWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    float defaultValue,
    UIResources* resources,
    UIState* uiState,
    Application* app
) {
    std::vector<std::shared_ptr<sf::Drawable>> drawables;
    
    Track* track = app->getTrack(trackName);
    if (!track) {
        return drawables;
    }
    
    const auto* automationPoints = track->getAutomationPoints(effectName, parameter);
    
    const float padding = 4.0f;
    const float usableHeight = rowSize.y - 2 * padding;
    
    auto valueToY = [&](float value) -> float {
        float normalizedValue = std::max(0.0f, std::min(value, 1.0f));
        return padding + (1.0f - normalizedValue) * usableHeight; // Invert Y axis
    };
    
    auto timeToX = [&](double time) -> float {
        return secondsToXPosition(bpm, beatWidth, time) + scrollOffset;
    };
    
    if (automationPoints && !automationPoints->empty()) {
        constexpr float pointRadius = 8.0f;
        sf::Color pointColor = app->resources.activeTheme->clip_color;
        sf::Color lineColor = pointColor;
        constexpr float lineHeight = 4.0f;
        
        // Sort points by time
        std::vector<Track::AutomationPoint> sortedPoints;
        for (const auto& point : *automationPoints) {
            if (point.time >= 0.0) { // Ignore default automation points at -1.0 seconds
                sortedPoints.push_back(point);
            }
        }
        std::sort(sortedPoints.begin(), sortedPoints.end(), 
                  [](const Track::AutomationPoint& a, const Track::AutomationPoint& b) {
                      return a.time < b.time;
                  });
        
        if (sortedPoints.empty()) {
            const float timelineWidth = rowSize.x + std::abs(scrollOffset) + 2000.0f;
            const float startX = scrollOffset - 1000.0f;
            
            float yPosition = valueToY(defaultValue);
            
            auto line = std::make_shared<sf::RectangleShape>();
            line->setSize({timelineWidth, lineHeight});
            line->setPosition({startX, yPosition});
            line->setFillColor(lineColor);
            drawables.push_back(line);
        } else if (sortedPoints.size() == 1) {
            const auto& point = sortedPoints[0];
            float yPosition = valueToY(point.value);
            const float timelineWidth = rowSize.x + std::abs(scrollOffset) + 2000.0f;
            const float startX = scrollOffset - 1000.0f;
            
            auto line = std::make_shared<sf::RectangleShape>();
            line->setSize({timelineWidth, lineHeight});
            line->setPosition({startX, yPosition - lineHeight / 2.0f});
            line->setFillColor(lineColor);
            drawables.push_back(line);
            
            float x = timeToX(point.time);
            if (x >= -pointRadius && x <= rowSize.x + pointRadius) {
                auto circle = std::make_shared<sf::CircleShape>(pointRadius);
                circle->setPosition({x - pointRadius, yPosition - pointRadius});
                circle->setFillColor(pointColor);
                circle->setOutlineThickness(1.0f);
                circle->setOutlineColor(sf::Color::Black);
                drawables.push_back(circle);
            }
        } else {
            // Draw connecting lines between points
            for (size_t i = 0; i < sortedPoints.size() - 1; ++i) {
                const auto& point1 = sortedPoints[i];
                const auto& point2 = sortedPoints[i + 1];
                
                float x1 = timeToX(point1.time);
                float y1 = valueToY(point1.value);
                float x2 = timeToX(point2.time);
                float y2 = valueToY(point2.value);
                
                if ((x1 >= -pointRadius && x1 <= rowSize.x + pointRadius) ||
                    (x2 >= -pointRadius && x2 <= rowSize.x + pointRadius)) {
                    
                    float lineLength = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
                    float angleRadians = std::atan2(y2 - y1, x2 - x1);
                    
                    auto line = std::make_shared<sf::RectangleShape>();
                    line->setSize({lineLength, lineHeight});
                    line->setPosition({x1, y1 - lineHeight / 2.0f});
                    line->setRotation(sf::radians(angleRadians));
                    line->setFillColor(lineColor);
                    drawables.push_back(line);
                }
            }
            
            // Draw extended automation lines
            if (!sortedPoints.empty()) {
                const float timelineExtension = 2000.0f;
                
                const auto& firstPoint = sortedPoints[0];
                float firstPointX = timeToX(firstPoint.time);
                float firstPointY = valueToY(firstPoint.value);
                
                float startX = scrollOffset - timelineExtension;
                float extendedWidth = firstPointX - startX;
                
                if (extendedWidth > 0 && firstPointX >= -pointRadius) {
                    auto preFirstLine = std::make_shared<sf::RectangleShape>();
                    preFirstLine->setSize({extendedWidth, lineHeight});
                    preFirstLine->setPosition({startX, firstPointY - lineHeight / 2.0f});
                    preFirstLine->setFillColor(lineColor);
                    drawables.push_back(preFirstLine);
                }
                
                const auto& lastPoint = sortedPoints.back();
                float lastPointX = timeToX(lastPoint.time);
                float lastPointY = valueToY(lastPoint.value);
                
                float endX = rowSize.x + std::abs(scrollOffset) + timelineExtension;
                float postExtendedWidth = endX - lastPointX;
                
                if (postExtendedWidth > 0 && lastPointX <= rowSize.x + pointRadius) {
                    auto postLastLine = std::make_shared<sf::RectangleShape>();
                    postLastLine->setSize({postExtendedWidth, lineHeight});
                    postLastLine->setPosition({lastPointX, lastPointY - lineHeight / 2.0f});
                    postLastLine->setFillColor(lineColor);
                    drawables.push_back(postLastLine);
                }
            }
            
            // Draw automation points
            for (const auto& point : sortedPoints) {
                float x = timeToX(point.time);
                float y = valueToY(point.value);
                
                if (x >= -pointRadius && x <= rowSize.x + pointRadius) {
                    auto circle = std::make_shared<sf::CircleShape>(pointRadius);
                    circle->setPosition({x - pointRadius, y - pointRadius});
                    circle->setFillColor(pointColor);
                    circle->setOutlineThickness(1.0f);
                    circle->setOutlineColor(sf::Color::Black);
                    drawables.push_back(circle);
                }
            }
        }
    } else {
        // No automation points
        const float timelineWidth = rowSize.x + std::abs(scrollOffset) + 2000.0f;
        const float startX = scrollOffset - 1000.0f;
        constexpr float lineHeight = 4.0f;
        
        // Use current parameter value
        float yPosition = valueToY(defaultValue);
        
        auto line = std::make_shared<sf::RectangleShape>();
        line->setSize({timelineWidth, lineHeight});
        line->setPosition({startX, yPosition});
        line->setFillColor(app->resources.activeTheme->clip_color);
        drawables.push_back(line);
    }
        
    return drawables;
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

    constexpr float linesPerSecond = 100.0f;
    constexpr float waveformScale = 0.9f;
    constexpr float peakThreshold = 0.001f;
    
    const int numPeaks = static_cast<int>(peaks.size());
    const double sourceFileDuration = getSourceFileDuration(clip);
    
    if (sourceFileDuration <= 0) return {};
    
    const int numLines = static_cast<int>(clip.duration * linesPerSecond);
    if (numLines <= 0) return {};
    
    sf::Color waveformColorWithAlpha = resources->activeTheme->wave_form_color;
    waveformColorWithAlpha.a = 180;

    const float lineHeightScale = clipSize.y * waveformScale;
    const float baseLineY = clipPosition.y + clipSize.y * 0.5f + verticalOffset;
    const float lineSpacing = clipSize.x / static_cast<float>(numLines);

    auto vertexArray = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
    vertexArray->resize(numLines * 2);

    size_t vertexIndex = 0;
    for (int i = 0; i < numLines; ++i) {
        const double timeInClip = (static_cast<double>(i) / static_cast<double>(numLines)) * clip.duration;
        const double timeInSource = clip.offset + timeInClip;
        const double peakIndexFloat = (timeInSource / sourceFileDuration) * (numPeaks - 1);
        const int peakIndex = static_cast<int>(peakIndexFloat);
        const float frac = static_cast<float>(peakIndexFloat - peakIndex);
        
        if (peakIndex >= numPeaks) break;
        
        float peakValue = peaks[peakIndex];
        if (peakIndex + 1 < numPeaks) {
            peakValue = std::fma(peaks[peakIndex + 1] - peaks[peakIndex], frac, peaks[peakIndex]);
        }
        
        if (peakValue > peakThreshold) {
            const float lineHeight = peakValue * lineHeightScale;
            const float lineX = clipPosition.x + i * lineSpacing;
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
        updateMouseCursor(); // Update cursor based on mouse position
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
        uiElements.automationClearButtons.clear();
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
    handleAutomationDragOperations();
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

void TimelineComponent::handleAutomationDragOperations() {
    if (!automationDragState.isDragging) return;
    
    bool isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    
    if (isLeftPressed) {
        // Continue dragging
        sf::Vector2f currentMousePos = app->ui->getMousePosition();
        auto* automationRow = containers[automationDragState.laneId + "_scrollable_row"];
        
        if (automationRow) {
            sf::Vector2f localMousePos = currentMousePos - automationRow->getPosition();
            sf::Vector2f rowSize = automationRow->getSize();
            
            // Calculate new time and value
            float newTime = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineState.timelineOffset, timelineState.timelineOffset);
            float newValue = 1.0f - (localMousePos.y / rowSize.y);
            newValue = std::max(0.0f, std::min(1.0f, newValue));
            
            // Snap to grid unless Shift is held
            bool shiftHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
            if (!shiftHeld) {
                auto [timeSigNum, timeSigDen] = app->getTimeSignature();
                auto measureLines = generateTimelineMeasures(100.f * app->uiState.timelineZoomLevel, timelineState.timelineOffset, rowSize, timeSigNum, timeSigDen, &app->resources);
                sf::Vector2f snapPos(localMousePos.x, localMousePos.y);
                float snapX = getNearestMeasureX(snapPos, measureLines);
                newTime = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, snapX - timelineState.timelineOffset, timelineState.timelineOffset);
            }
            
            if (newTime >= 0.0f) {
                Track* track = app->getTrack(automationDragState.trackName);
                if (track) {
                    track->moveAutomationPoint(
                        automationDragState.effectName, 
                        automationDragState.parameterName, 
                        automationDragState.startTime, 
                        newTime, 
                        newValue
                    );
                    automationDragState.startTime = newTime; // Update for next move event
                }
            }
        }
    } else {
        // End dragging when mouse button released
        automationDragState.isDragging = false;
    }
}

void TimelineComponent::updateDragState() {
    if (!features.enableClipDragging) return;
    
    sf::Vector2f currentMousePos = app->ui->getMousePosition();
    
    if (isResizing && (clipEndDrag || clipStartDrag) && selectedClip && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        sf::Vector2f trackRowPos;
        bool foundTrackRow = false;
        
        for (const auto& track : app->getAllTracks()) {
            const std::string rowKey = track->getName() + "_scrollable_row";
            if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                // Check if this track contains our selected clip
                const auto& clips = track->getClips();
                for (const auto& clip : clips) {
                    if (&clip == selectedClip) {
                        trackRowPos = rowIt->second->getPosition();
                        foundTrackRow = true;
                        break;
                    }
                }
                if (foundTrackRow) break;
            }
        }
        
        if (foundTrackRow) {
            sf::Vector2f localMousePos = currentMousePos - trackRowPos;
            const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
            const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
            float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
            
            if (clipEndDrag) {
                // Resize from end - only change duration, keep start time fixed
                // Calculate delta from where drag started
                double mouseDelta = mouseTimeInTrack - resizeDragStartMouseTime;
                double snappedDelta = snapToGrid(mouseDelta) - mouseDelta; // Get snap adjustment
                double totalDelta = mouseDelta + snappedDelta;
                
                double newDuration = originalClipDuration + totalDelta;
                
                // Get source file duration for bounds checking
                double sourceFileDuration = getSourceFileDuration(*selectedClip);
                double maxAllowedDuration = sourceFileDuration - selectedClip->offset;
                
                newDuration = std::max(0.1, std::min(newDuration, maxAllowedDuration));
                
                if (std::abs(newDuration - selectedClip->duration) > 0.001) {
                    selectedClip->duration = newDuration;
                    selectedClipEnd = selectedClip->startTime + selectedClip->duration;
                    invalidateClipWaveform(*selectedClip);
                    
                    timelineState.virtualCursorTime = selectedClip->startTime + selectedClip->duration;
                    timelineState.showVirtualCursor = true;
                    timelineState.virtualCursorVisible = true;
                }
            } else if (clipStartDrag) {
                // Resize from start - keep end time fixed, change start time, duration, and offset
                // Calculate delta from where drag started
                double mouseDelta = mouseTimeInTrack - resizeDragStartMouseTime;
                double snappedDelta = snapToGrid(mouseDelta) - mouseDelta; // Get snap adjustment
                double totalDelta = mouseDelta + snappedDelta;
                
                double fixedEndTime = originalClipStartTime + originalClipDuration; // Use original values
                double newStartTime = originalClipStartTime + totalDelta;
                
                // Allow slight negative values for smoother interaction at timeline start
                newStartTime = std::max(-0.1, newStartTime);
                
                // Clamp to zero if very close to prevent actual negative start times
                if (newStartTime < 0.05 && newStartTime > -0.05) {
                    newStartTime = 0.0;
                }
                
                double newDuration = fixedEndTime - newStartTime;
                
                // Calculate how much we're shifting the start time from original
                double startTimeShift = newStartTime - originalClipStartTime;
                double newOffset = originalClipOffset + startTimeShift; // Offset moves with start time
                
                // Get source file duration for bounds checking
                double sourceFileDuration = getSourceFileDuration(*selectedClip);
                
                // Validate bounds: offset must be >= 0 and offset + duration <= sourceFileDuration
                if (newOffset < 0.0) {
                    // If offset would be negative, limit start time so offset = 0
                    // When offset = 0: newOffset = originalClipOffset + startTimeShift = 0
                    // So: startTimeShift = -originalClipOffset
                    // And: newStartTime = originalClipStartTime + startTimeShift
                    newOffset = 0.0;
                    double maxLeftShift = -originalClipOffset;
                    newStartTime = originalClipStartTime + maxLeftShift;
                    newStartTime = std::max(0.0, newStartTime); // Can't go negative on timeline
                    newDuration = fixedEndTime - newStartTime;
                }
                
                if (newOffset + newDuration > sourceFileDuration) {
                    newDuration = sourceFileDuration - newOffset;
                    newStartTime = fixedEndTime - newDuration;
                }
                
                if (newDuration > 0.1 && newStartTime >= 0.0 && newOffset >= 0.0) {
                    newStartTime = std::max(0.0, newStartTime);
                    newDuration = fixedEndTime - newStartTime;
                    if (newDuration > 0.1) {
                        selectedClip->startTime = newStartTime;
                        selectedClip->duration = newDuration;
                        selectedClip->offset = newOffset;
                        selectedClipEnd = selectedClip->startTime + selectedClip->duration;
                        invalidateClipWaveform(*selectedClip);
                    
                        timelineState.virtualCursorTime = selectedClip->startTime;
                        timelineState.showVirtualCursor = true;
                        timelineState.virtualCursorVisible = true;
                    }
                }
            }
        }
        return;
    }
    
    // Handle MIDI clip resizing
    if (isMIDIResizing && (midiClipEndDrag || midiClipStartDrag) && selectedMIDIClipInfo.hasSelection && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        auto* selectedMIDIClip = getSelectedMIDIClip();
        if (selectedMIDIClip) {
            sf::Vector2f trackRowPos;
            bool foundTrackRow = false;
            
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
                    if (track->getName() == selectedMIDIClipInfo.trackName) {
                        trackRowPos = rowIt->second->getPosition();
                        foundTrackRow = true;
                        break;
                    }
                }
            }
            
            if (foundTrackRow) {
                sf::Vector2f currentMousePos = app->ui->getMousePosition();
                sf::Vector2f localMousePos = currentMousePos - trackRowPos;
                const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
                const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
                float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                
                if (midiClipEndDrag) {
                    double mouseDelta = mouseTimeInTrack - resizeDragStartMouseTime;
                    double snappedDelta = snapToGrid(mouseDelta) - mouseDelta;
                    double totalDelta = mouseDelta + snappedDelta;
                    
                    double newDuration = originalMIDIClipDuration + totalDelta;
                    newDuration = std::max(0.1, newDuration); // Minimum duration
                    
                    if (std::abs(newDuration - selectedMIDIClip->duration) > 0.001) {
                        selectedMIDIClip->duration = newDuration;
                        selectedMIDIClipInfo.duration = newDuration;
                        
                        timelineState.virtualCursorTime = selectedMIDIClip->startTime + selectedMIDIClip->duration;
                        timelineState.showVirtualCursor = true;
                        timelineState.virtualCursorVisible = true;
                    }
                } else if (midiClipStartDrag) {
                    double mouseDelta = mouseTimeInTrack - resizeDragStartMouseTime;
                    double snappedDelta = snapToGrid(mouseDelta) - mouseDelta;
                    double totalDelta = mouseDelta + snappedDelta;
                    
                    double fixedEndTime = originalMIDIClipStartTime + originalMIDIClipDuration;
                    double newStartTime = originalMIDIClipStartTime + totalDelta;
                    newStartTime = std::max(0.0, newStartTime); // Can't go negative
                    
                    double newDuration = fixedEndTime - newStartTime;
                    
                    if (newDuration > 0.1 && newStartTime >= 0.0) {
                        selectedMIDIClip->startTime = newStartTime;
                        selectedMIDIClip->duration = newDuration;
                        selectedMIDIClipInfo.startTime = newStartTime;
                        selectedMIDIClipInfo.duration = newDuration;
                        
                        timelineState.virtualCursorTime = selectedMIDIClip->startTime;
                        timelineState.showVirtualCursor = true;
                        timelineState.virtualCursorVisible = true;
                    }
                }
            }
        }
        return;
    }
    
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        clipEndDrag = false;
        clipStartDrag = false;
        isResizing = false;
        midiClipEndDrag = false;
        midiClipStartDrag = false;
        isMIDIResizing = false;
    }
    
    if (isResizing || isMIDIResizing) {
        return;
    }
    
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
            // Auto-following playhead
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
    
    // Don't update virtual cursor during resize - it should stay locked to clip edge
    if (isResizing) {
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

void TimelineComponent::resetDragState() {
    dragState.isDraggingClip = false;
    dragState.isDraggingAudioClip = false;
    dragState.isDraggingMIDIClip = false;
    dragState.clipSelectedForDrag = false;
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
    }
    
    // Copy selected MIDIClip
    MIDIClip* selectedMIDIClip = getSelectedMIDIClip();
    if (selectedMIDIClip) {
        clipboardState.copiedMIDIClips.push_back(*selectedMIDIClip);
        clipboardState.hasClipboard = true;
    }
    
    if (!clipboardState.hasClipboard) {
        // No clips selected to copy
    }
}

void TimelineComponent::pasteClips() {
    if (!clipboardState.hasClipboard) {
        // No clips in clipboard to paste
        return;
    }
    
    double cursorPosition = app->getPosition();
    std::string currentTrack = app->getSelectedTrack();
    
    if (currentTrack.empty()) {
        // No track selected for pasting
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

void TimelineComponent::updateAutomationLaneLabels() {
    // Update automation lane labels and values
    const auto& allTracks = app->getAllTracks();
    for (const auto& trackPtr : allTracks) {
        auto* track = trackPtr.get();
        
        // Update potential automation lane label
        if (track->hasPotentialAutomation()) {
            auto potentialAuto = track->getPotentialAutomation();
            std::string effectName = potentialAuto.first;
            std::string parameterName = potentialAuto.second;            
            std::string key = track->getName() + "_potential"; // Key for potential automation lane
            
            // Update label if it exists
            auto labelIt = uiElements.automationLaneLabels.find(key);
            if (labelIt != uiElements.automationLaneLabels.end()) {
                std::string labelText = "[+] " + effectName + " - " + parameterName;
                labelIt->second->setString(labelText);
                
                auto [currentEffectName, currentParameterName, currentValue] = getCurrentAutomationParameter(track);
                std::string valueText = std::to_string(static_cast<int>(currentValue * 100)) + "%";
                
                // Find the value text element (assuming it's the second text in the row)
                auto rowIt = uiElements.automationLaneRows.find(key);
                if (rowIt != uiElements.automationLaneRows.end()) {
                    // Update the automation line position based on the current value
                }
            }
        }
        
        // Update regular automation lane labels (remove [+] prefix)
        const auto& automatedParams = track->getAutomatedParameters();
        for (const auto& [effectName, parameterName] : automatedParams) {
            std::string key = track->getName() + "_" + effectName + "_" + parameterName; // Key for regular automation lane
            
            auto labelIt = uiElements.automationLaneLabels.find(key);
            if (labelIt != uiElements.automationLaneLabels.end()) {
                std::string labelText = effectName + " - " + parameterName; // No [+] prefix for regular automation
                labelIt->second->setString(labelText);
            }
        }
    }
}

// Cursor management implementation
void TimelineComponent::initializeCursors() {
    try {
        resizeCursorH = std::make_unique<sf::Cursor>(sf::Cursor::Type::SizeHorizontal);
        textCursor = std::make_unique<sf::Cursor>(sf::Cursor::Type::Text);
        handCursor = std::make_unique<sf::Cursor>(sf::Cursor::Type::Hand);
        cursorsEnabled = true;
    } catch (const std::exception& e) {
        DEBUG_PRINT("[CURSOR] Failed to initialize cursors, disabling cursor management: " << e.what());
        cursorsEnabled = false;
    }
}

void TimelineComponent::setCursor(sf::Cursor::Type cursorType) {
    if (!cursorsEnabled || currentCursorType == cursorType) return;
    
    try {
        sf::Cursor* cursor = nullptr;
        switch (cursorType) {
            case sf::Cursor::Type::SizeHorizontal:
                cursor = resizeCursorH.get();
                break;
            case sf::Cursor::Type::Text:
                cursor = textCursor.get();
                break;
            case sf::Cursor::Type::Hand:
                cursor = handCursor.get();
                break;
            default:
                // Use system default arrow cursor - create it safely
                try {
                    sf::Cursor defaultCursor(sf::Cursor::Type::Arrow);
                    const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(defaultCursor);
                    currentCursorType = cursorType;
                } catch (const std::exception& e) {
                    DEBUG_PRINT("[CURSOR] Failed to set default cursor: " << e.what());
                }
                return;
        }
        
        if (cursor) {
            const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(*cursor);
            currentCursorType = cursorType;
        }
    } catch (const std::exception& e) {
        DEBUG_PRINT("[CURSOR] Failed to set cursor: " << e.what());
        // Fallback: don't change cursor if it fails
    }
}

void TimelineComponent::updateMouseCursor() {
    if (!cursorsEnabled) return; // Skip if cursors are disabled
    
    sf::Vector2f mousePos = app->ui->getMousePosition();
    sf::Cursor::Type newCursorType = sf::Cursor::Type::Arrow;
    
    // Check if mouse is over any clip resize zones
    for (const auto& track : app->getAllTracks()) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        if (auto rowIt = containers.find(rowKey); rowIt != containers.end() && rowIt->second) {
            sf::Vector2f trackRowPos = rowIt->second->getPosition();
            sf::Vector2f localMousePos = mousePos - trackRowPos;
            
            const auto& clips = track->getClips();
            for (const auto& clip : clips) {
                const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
                const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
                const float clipWidthPixels = clip.duration * pixelsPerSecond;
                const float clipXPosition = (clip.startTime * pixelsPerSecond) + timelineState.timelineOffset;
                const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, rowIt->second->getSize().y});
                
                if (clipRect.contains(localMousePos)) {
                    float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                    ResizeZone zone = getResizeZone(clip, mouseTimeInTrack, pixelsPerSecond);
                    
                    if (zone != ResizeZone::None) {
                        newCursorType = sf::Cursor::Type::SizeHorizontal;
                        goto cursor_found;
                    }
                }
            }
            
            // Check MIDI clips for resize zones
            if (track->getType() == Track::TrackType::MIDI) {
                MIDITrack* midiTrack = static_cast<MIDITrack*>(track.get());
                const auto& midiClips = midiTrack->getMIDIClips();
                for (const auto& midiClip : midiClips) {
                    const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
                    const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
                    const float clipWidthPixels = midiClip.duration * pixelsPerSecond;
                    const float clipXPosition = (midiClip.startTime * pixelsPerSecond) + timelineState.timelineOffset;
                    const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, rowIt->second->getSize().y});
                    
                    if (clipRect.contains(localMousePos)) {
                        float mouseTimeInTrack = (localMousePos.x - timelineState.timelineOffset) / pixelsPerSecond;
                        ResizeZone zone = getResizeZone(midiClip, mouseTimeInTrack, pixelsPerSecond);
                        
                        if (zone != ResizeZone::None) {
                            newCursorType = sf::Cursor::Type::SizeHorizontal;
                            goto cursor_found;
                        }
                    }
                }
            }
        }
    }
    
    cursor_found:
    setCursor(newCursorType);
}

bool TimelineComponent::isMouseOverResizeZone(const AudioClip& clip, float mouseTimeInTrack) const {
    const float beatWidth = 100.f * app->uiState.timelineZoomLevel;
    const float pixelsPerSecond = (beatWidth * app->getBpm()) / 60.0f;
    return getResizeZone(clip, mouseTimeInTrack, pixelsPerSecond) != ResizeZone::None;
}

TimelineComponent::ResizeZone TimelineComponent::getResizeZone(const AudioClip& clip, float mouseTimeInTrack, float pixelsPerSecond) const {
    const float resizeThresholdPixels = 10.0f; // 10 pixels
    const double resizeThresholdTime = resizeThresholdPixels / pixelsPerSecond;
    
    const double clipStart = clip.startTime;
    const double clipEnd = clip.startTime + clip.duration;
    
    bool isNearStart = std::abs(mouseTimeInTrack - clipStart) <= resizeThresholdTime;
    bool isNearEnd = std::abs(mouseTimeInTrack - clipEnd) <= resizeThresholdTime;
    
    // Special handling for very small clips (less than 20 pixels wide)
    const double clipWidthPixels = clip.duration * pixelsPerSecond;
    if (clipWidthPixels < 20.0f) {
        // For small clips, expand the threshold and prefer the edge closest to mouse
        const double smallClipThresholdTime = std::max(resizeThresholdTime, clip.duration * 0.4);
        isNearStart = std::abs(mouseTimeInTrack - clipStart) <= smallClipThresholdTime;
        isNearEnd = std::abs(mouseTimeInTrack - clipEnd) <= smallClipThresholdTime;
    }
    
    // If both are detected, choose the closest edge
    if (isNearStart && isNearEnd) {
        double distToStart = std::abs(mouseTimeInTrack - clipStart);
        double distToEnd = std::abs(mouseTimeInTrack - clipEnd);
        return (distToStart <= distToEnd) ? ResizeZone::Start : ResizeZone::End;
    }
    
    // Priority for edge cases: if very close to start (within 2 pixels), prefer start resize
    const double priorityThresholdTime = 2.0f / pixelsPerSecond;
    if (std::abs(mouseTimeInTrack - clipStart) <= priorityThresholdTime) {
        return ResizeZone::Start;
    }
    
    // Priority for edge cases: if very close to end (within 2 pixels), prefer end resize  
    if (std::abs(mouseTimeInTrack - clipEnd) <= priorityThresholdTime) {
        return ResizeZone::End;
    }
    
    if (isNearStart) return ResizeZone::Start;
    if (isNearEnd) return ResizeZone::End;
    return ResizeZone::None;
}

TimelineComponent::ResizeZone TimelineComponent::getResizeZone(const MIDIClip& clip, float mouseTimeInTrack, float pixelsPerSecond) const {
    const float resizeThresholdPixels = 10.0f;
    const double resizeThresholdTime = resizeThresholdPixels / pixelsPerSecond;
    
    const double clipStart = clip.startTime;
    const double clipEnd = clip.startTime + clip.duration;
    
    bool isNearStart = std::abs(mouseTimeInTrack - clipStart) <= resizeThresholdTime;
    bool isNearEnd = std::abs(mouseTimeInTrack - clipEnd) <= resizeThresholdTime;
    
    const double clipWidthPixels = clip.duration * pixelsPerSecond;
    if (clipWidthPixels < 20.0f) {
        const double smallClipThresholdTime = std::max(resizeThresholdTime, clip.duration * 0.4);
        isNearStart = std::abs(mouseTimeInTrack - clipStart) <= smallClipThresholdTime;
        isNearEnd = std::abs(mouseTimeInTrack - clipEnd) <= smallClipThresholdTime;
    }
    
    if (isNearStart && isNearEnd) {
        double distToStart = std::abs(mouseTimeInTrack - clipStart);
        double distToEnd = std::abs(mouseTimeInTrack - clipEnd);
        return (distToStart <= distToEnd) ? ResizeZone::Start : ResizeZone::End;
    }
    
    const double priorityThresholdTime = 2.0f / pixelsPerSecond;
    if (std::abs(mouseTimeInTrack - clipStart) <= priorityThresholdTime) {
        return ResizeZone::Start;
    }
    
    if (std::abs(mouseTimeInTrack - clipEnd) <= priorityThresholdTime) {
        return ResizeZone::End;
    }
    
    if (isNearStart) return ResizeZone::Start;
    if (isNearEnd) return ResizeZone::End;
    return ResizeZone::None;
}

// Grid snapping implementation
double TimelineComponent::snapToGrid(double timeValue, bool forceSnap) const {
    bool shouldSnap = forceSnap || !isShiftPressed();
    if (!shouldSnap) return timeValue;
    
    const double bpm = app->getBpm();
    const double beatDuration = 60.0 / bpm;
    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
    
    // Snap to beat subdivisions (16th notes by default)
    const double snapResolution = beatDuration / 4.0; // 16th notes
    
    return std::round(timeValue / snapResolution) * snapResolution;
}

bool TimelineComponent::isShiftPressed() const {
    return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || 
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
}

void TimelineComponent::handleTrackRenaming() {
    // Find active textbox first to avoid iterator invalidation
    std::string activeTrackName;
    TextBox* activeTextBox = nullptr;
    
    for (auto& [trackName, textBox] : uiElements.trackNameTextBoxes) {
        if (textBox && textBox->isActive()) {
            activeTrackName = trackName;
            activeTextBox = textBox;
            break; // Only one can be active at a time
        }
    }
    
    if (!activeTextBox) return; // No active textbox
    
    // Skip Master track renaming (Master track should not be renameable)
    if (activeTrackName == "Master") {
        activeTextBox->setActive(false);
        return;
    }
    
    // Check if Enter was pressed to confirm rename
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) {
        std::string newName = activeTextBox->getText();
        
        // Validate new name
        if (!newName.empty() && newName != activeTrackName) {
            auto track = app->getTrack(activeTrackName);
            if (track) {
                // Deactivate first to prevent issues during track modification
                activeTextBox->setActive(false);
                
                // Rename the track (this might trigger UI rebuilds)
                track->setName(newName);
                
                // Remove old textbox reference and add new one
                uiElements.trackNameTextBoxes.erase(activeTrackName);
                uiElements.trackNameTextBoxes[newName] = activeTextBox;
                
                return; // Exit early after successful rename
            }
        }
        
        // If we get here, the rename was invalid - just deactivate
        activeTextBox->setActive(false);
    }
    
    // Check if Escape was pressed to cancel rename
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
        // Reset to original name and deactivate
        activeTextBox->setText(activeTrackName);
        activeTextBox->setActive(false);
    }
}

// Static member definition
TimelineComponent* TimelineComponent::instance = nullptr;
bool TimelineComponent::clipEndDrag = false;
bool TimelineComponent::clipStartDrag = false;
bool TimelineComponent::isResizing = false;

// Plugin interface for TimelineComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(TimelineComponent)