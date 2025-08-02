#pragma once

#include "MULOComponent.hpp"
#include "Application.hpp"
#include <chrono>

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

    // Volume conversion helpers  
    static float decibelsToFloat(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    static float floatToDecibels(float value) {
        return 20.0f * std::log10(std::max(value, 0.001f)); // Prevent log(0)
    }
};

inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(
    double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, 
    const std::vector<AudioClip>& clips, float verticalOffset, 
    UIResources* resources, UIState* uiState, const AudioClip* selectedClip,
    const std::string& currentTrackName, const std::string& selectedTrackName
);

inline std::shared_ptr<sf::Drawable> getPlayHead(
    double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize
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
}

void TimelineComponent::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;
    masterTrackElement = masterTrack();
    // Create scrollableColumn and add all track rows
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

    layout = row(
        Modifier()
            .align(Align::RIGHT), 
        contains{
            column(Modifier().setColor(app->resources.activeTheme->middle_color).align(Align::RIGHT), contains{
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
    if (!isVisible()) return;
    // Calculate delta time for frame-rate independent updates
    auto currentTime = std::chrono::steady_clock::now();
    
    if (!firstFrame) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastFrameTime);
        deltaTime = duration.count() / 1000000.0f; // Convert to seconds
        
        // Cap delta time to prevent large jumps (e.g., when debugging or window focus lost)
        constexpr float maxDeltaTime = 1.0f / 30.0f; // 30 FPS minimum
        deltaTime = std::min(deltaTime, maxDeltaTime);
    } else {
        deltaTime = 1.0f / 60.0f; // Assume 60 FPS for first frame
        firstFrame = false;
    }
    
    lastFrameTime = currentTime;
    
    handleCustomUIElements();
}

bool TimelineComponent::handleEvents() {
    bool forceUpdate = app->isPlaying();

    // Sync sliders to engine when component becomes visible
    if (isVisible() && !wasVisible) {
        syncSlidersToEngine();
        wasVisible = true;
    } else if (!isVisible()) {
        wasVisible = false;
    }

    // Master track controls - optimized
    if (muteMasterButton && muteMasterButton->isClicked()) {
        auto* masterTrack = app->getMasterTrack();
        masterTrack->toggleMute();
        muteMasterButton->m_modifier.setColor(
            masterTrack->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
        );
        return true;
    }
    
    // Master volume handling with tolerance check - only if timeline is visible
    if (isVisible()) {
        const float newMasterVolDb = floatToDecibels(masterVolumeSlider->getValue());
        auto* masterTrack = app->getMasterTrack();
        constexpr float volumeTolerance = 0.001f;
        
        if (std::abs(masterTrack->getVolume() - newMasterVolDb) > volumeTolerance) {
            masterTrack->setVolume(newMasterVolDb);
            forceUpdate = true;
        }
    }

    // Track synchronization - optimized with set operations
    const auto& allTracks = app->getAllTracks();
    std::set<std::string> engineTrackNames, uiTrackNames;
    
    // Build engine track names efficiently
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name != "Master") {
            engineTrackNames.emplace(name);
        }
    }
    
    // Build UI track names efficiently
    for (const auto& [name, _] : trackMuteButtons) {
        uiTrackNames.emplace(name);
    }
    for (const auto& [name, _] : trackVolumeSliders) {
        uiTrackNames.emplace(name);
    }
    
    // Clear UI if tracks changed
    if (engineTrackNames != uiTrackNames) {
        if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
            timelineIt->second->clear();
        }
        trackMuteButtons.clear();
        trackVolumeSliders.clear();
    }
    
    // Process individual tracks - optimized
    for (const auto& t : allTracks) {
        const auto& name = t->getName();
        if (name == "Master") continue;
        
        // Add new tracks if needed
        const bool hasMuteButton = trackMuteButtons.find(name) != trackMuteButtons.end();
        const bool hasVolumeSlider = trackVolumeSliders.find(name) != trackVolumeSliders.end();
        
        if (!hasMuteButton && !hasVolumeSlider) {
            if (auto timelineIt = containers.find("timeline"); timelineIt != containers.end() && timelineIt->second) {
                timelineIt->second->addElements({
                    spacer(Modifier().setfixedHeight(4.f)),
                    track(name, Align::TOP | Align::LEFT, t->getVolume(), t->getPan())
                });
                forceUpdate = true;
            }
        }
        
        // Handle mute button clicks
        if (auto muteBtnIt = trackMuteButtons.find(name); 
            muteBtnIt != trackMuteButtons.end() && muteBtnIt->second && muteBtnIt->second->isClicked()) {
            t->toggleMute();
            muteBtnIt->second->m_modifier.setColor(
                t->isMuted() ? app->resources.activeTheme->mute_color : app->resources.activeTheme->not_muted_color
            );
            forceUpdate = true;
        }
        
        // Handle volume slider changes - only if timeline is visible (not hidden by mixer)
        if (isVisible()) {
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

    // Keyboard handling with static state - optimized
    static bool prevCtrl = false, prevPlus = false, prevMinus = false, prevBackspace = false;
    const bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
    const bool plus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Equal);
    const bool minus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Hyphen);
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);

    // Remove selected clip on backspace - optimized
    if (selectedClip && backspace && !prevBackspace) {
        // Find the track containing the selected clip - track-aware deletion
        for (const auto& t : allTracks) {
            // Only check the selected track
            if (t->getName() != selectedTrack) continue;
            
            const auto& clips = t->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                const auto& clip = clips[i];
                if (clip.startTime == selectedClip->startTime &&
                    clip.duration == selectedClip->duration &&
                    clip.sourceFile == selectedClip->sourceFile) {
                    t->removeClip(static_cast<int>(i));
                    selectedClip = nullptr;
                    forceUpdate = true;
                    std::cout << "Removed selected clip from track '" << t->getName() << "'\n";
                    goto done_clip_remove;
                }
            }
        }
        done_clip_remove: ;
    }

    // Zoom handling - optimized with constants
    constexpr float zoomSpeed = 0.2f;
    constexpr float maxZoom = 5.0f;
    constexpr float minZoom = 0.1f;
    
    if (ctrl && plus && !prevPlus) {
        const float newZoom = std::min(maxZoom, app->uiState.timelineZoomLevel + zoomSpeed);
        if (newZoom != app->uiState.timelineZoomLevel) {
            // Get current scroll offset
            const float currentOffset = timelineOffset;
            
            // Get mouse position relative to timeline
            const sf::Vector2f mousePos = app->ui->getMousePosition();
            
            // Find a timeline row to get position reference
            sf::Vector2f timelineRowPos(0.f, 0.f);
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    timelineRowPos = rowIt->second->getPosition();
                    break;
                }
            }
            
            // Calculate mouse position relative to timeline content
            const float mouseXInTimeline = mousePos.x - timelineRowPos.x;
            
            // Apply zoom
            app->uiState.timelineZoomLevel = newZoom;
            
            // Calculate zoom factor ratio
            const float zoomRatio = newZoom / (newZoom - zoomSpeed);
            
            // Calculate new offset to keep mouse position fixed
            timelineOffset = mouseXInTimeline - (mouseXInTimeline - currentOffset) * zoomRatio;
            
            // Immediately apply the new offset to all scrollable rows
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                    scrollableRow->setOffset(std::min(0.f, timelineOffset));
                }
            }
            
            forceUpdate = true;
        }
    }
    
    if (ctrl && minus && !prevMinus) {
        const float newZoom = std::max(minZoom, app->uiState.timelineZoomLevel - zoomSpeed);
        if (newZoom != app->uiState.timelineZoomLevel) {
            // Get current scroll offset
            const float currentOffset = timelineOffset;
            
            // Get mouse position relative to timeline
            const sf::Vector2f mousePos = app->ui->getMousePosition();
            
            // Find a timeline row to get position reference
            sf::Vector2f timelineRowPos(0.f, 0.f);
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    timelineRowPos = rowIt->second->getPosition();
                    break;
                }
            }
            
            // Calculate mouse position relative to timeline content
            const float mouseXInTimeline = mousePos.x - timelineRowPos.x;
            
            // Apply zoom
            app->uiState.timelineZoomLevel = newZoom;
            
            // Calculate zoom factor ratio
            const float zoomRatio = newZoom / (newZoom + zoomSpeed);
            
            // Calculate new offset to keep mouse position fixed
            timelineOffset = mouseXInTimeline - (mouseXInTimeline - currentOffset) * zoomRatio;
            
            // Immediately apply the new offset to all scrollable rows
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                    scrollableRow->setOffset(std::min(0.f, timelineOffset));
                }
            }

            
            
            forceUpdate = true;
        }
    }

    // Scroll handling - optimized
    const int vertScroll = app->ui->getVerticalScrollDelta();
    const int horizScroll = app->ui->getHorizontalScrollDelta();
    
    // if (ctrl) app->ui->setInputBlocked(true);
    // else app->ui->setInputBlocked(false);
    
    // Handle Ctrl+scroll for zooming
    if (ctrl && vertScroll != 0) {
        constexpr float zoomSpeed = 0.1f;
        constexpr float maxZoom = 20.0f;
        constexpr float minZoom = 0.1f;
        
        // Scroll up = zoom in, scroll down = zoom out
        const float zoomDelta = (vertScroll < 0) ? zoomSpeed : -zoomSpeed;
        const float newZoom = std::clamp(app->uiState.timelineZoomLevel + zoomDelta, minZoom, maxZoom);
        
        if (newZoom != app->uiState.timelineZoomLevel) {
            const float currentOffset = timelineOffset;
            const float oldZoom = app->uiState.timelineZoomLevel;
            
            // Get mouse position relative to timeline
            const sf::Vector2f mousePos = app->ui->getMousePosition();
            
            // Find a timeline row to get position reference
            sf::Vector2f timelineRowPos(0.f, 0.f);
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    timelineRowPos = rowIt->second->getPosition();
                    break;
                }
            }
            
            // Calculate mouse position relative to timeline content
            const float mouseXInTimeline = mousePos.x - timelineRowPos.x;
            
            // Apply zoom
            app->uiState.timelineZoomLevel = newZoom;
            
            // Calculate zoom factor ratio
            const float zoomRatio = newZoom / oldZoom;
            
            // Calculate new offset to keep mouse position fixed
            timelineOffset = mouseXInTimeline - (mouseXInTimeline - currentOffset) * zoomRatio;
            
            // Immediately apply the new offset to all scrollable rows
            for (const auto& track : app->getAllTracks()) {
                const std::string rowKey = track->getName() + "_scrollable_row";
                auto rowIt = containers.find(rowKey);
                if (rowIt != containers.end() && rowIt->second) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(rowIt->second);
                    scrollableRow->setOffset(std::min(0.f, timelineOffset));
                }
            }
            
            forceUpdate = true;
        }
    }

    // Update state for next frame
    prevCtrl = ctrl;
    prevPlus = plus;
    prevMinus = minus;
    prevBackspace = backspace;

    app->ui->resetScrollDeltas();

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

    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(96)
            .align(Align::LEFT | Align::BOTTOM)
            .onLClick([&](){
                selectedTrack = "Master";
            })
            .onRClick([&](){
                selectedTrack = "Master";
            }),
    contains{
        column(
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
        }, "Master_Track_Column")
    }, "Master_Track");
}

uilo::Row* TimelineComponent::track(
    const std::string& trackName, 
    Align alignment, 
    float volume,
    float pan
) {
    trackMuteButtons[trackName] = button(
        Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(app->resources.activeTheme->not_muted_color),
        ButtonStyle::Rect,
        "mute",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mute_" + trackName
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
        contains {
            // contains nothing, really just to get the offset from scroll
        }, trackName + "_scrollable_row"
    );
    containers[trackName + "_scrollable_row"] = scrollableRowElement;

    scrollableRowElement->m_modifier.onLClick([this, trackName](){
        // Only add a clip if ctrl is pressed
        if (!(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) 
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)))
            return;

        auto track = app->getTrack(trackName);
        if (!track) return;

        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        auto* trackRow = containers[trackName + "_scrollable_row"];
        if (!trackRow) return;

        sf::Vector2f trackRowPos = trackRow->getPosition();
        sf::Vector2f localMousePos = globalMousePos - trackRowPos;

        auto lines = generateTimelineMeasures(
            100.f * app->uiState.timelineZoomLevel,
            timelineOffset,
            trackRow->getSize(),
            4, 4,
            &app->resources
        );

        float snapX = getNearestMeasureX(localMousePos, lines);
        float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, snapX - timelineOffset, timelineOffset);

        if (track->getReferenceClip()) {
            AudioClip* newClip = track->getReferenceClip();
            track->addClip(AudioClip(
                newClip->sourceFile,
                timePosition,
                0.0,
                newClip->duration,
                1.0f
            ));
        }

        forceUpdate = true;
        std::cout << "Added clip to track '" << track->getName() << "' at time: " << timePosition << " seconds" << std::endl;
    });

    scrollableRowElement->m_modifier.onRClick([this, trackName](){
        // Only remove a clip if ctrl is pressed
        if (!(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) 
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)))
            return;

        auto track = app->getTrack(trackName);
        if (!track) return;

        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        auto* trackRow = containers[trackName + "_scrollable_row"];
        if (!trackRow) return;

        sf::Vector2f trackRowPos = trackRow->getPosition();
        sf::Vector2f localMousePos = globalMousePos - trackRowPos;

        float timePosition = xPosToSeconds(app->getBpm(), 100.f * app->uiState.timelineZoomLevel, localMousePos.x - timelineOffset, timelineOffset);

        auto clips = track->getClips();
        for (size_t i = 0; i < clips.size(); ++i) {
            if (timePosition >= clips[i].startTime && timePosition <= (clips[i].startTime + clips[i].duration)) {
                std::cout << "Removed clip from track '" << track->getName() << "' at time: " << clips[i].startTime << " seconds" << std::endl;
                track->removeClip(i);
                forceUpdate = true;
                break;
            }
        }
    });

    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(96)
            .align(alignment)
            .onLClick([&](){
                selectedTrack = trackName;
            })
            .onRClick([&](){
                selectedTrack = trackName;
            }),
    contains{
        scrollableRowElement,

        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(196)
                .setColor(app->resources.activeTheme->track_color),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            row(
                Modifier().align(Align::RIGHT).setHighPriority(true),
            contains{
                spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

                column(
                    Modifier(),
                contains{
                    text(
                        Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        trackName,
                        app->resources.dejavuSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        trackMuteButtons[trackName]
                    }),
                }),

                trackVolumeSliders[trackName],

                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            }),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        }, trackName + "_label")
    }, trackName + "_track_row");
}

void TimelineComponent::handleCustomUIElements() {
    // Handle selection logic for clips
    static bool prevCtrlPressed = false;
    static bool prevBackspace = false;
    static std::string prevSelectedTrack = "";
    
    const bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    const bool backspace = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
    
    // Clear selected clip if the track changed
    if (selectedTrack != prevSelectedTrack) {
        selectedClip = nullptr;
        prevSelectedTrack = selectedTrack;
    }
    
    auto timelineIt = containers.find("timeline");
    if (timelineIt == containers.end() || !timelineIt->second) return;
    auto* timelineElement = static_cast<ScrollableColumn*>(timelineIt->second);

    const auto& allTracks = app->getAllTracks();
    if (allTracks.empty()) return;

    float newMasterOffset = timelineOffset;

            // Cache commonly used values
    const double bpm = app->getBpm();
    const float zoomLevel = app->uiState.timelineZoomLevel;
    const float beatWidth = 100.f * zoomLevel;
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const bool isPlaying = app->isPlaying();
    const sf::Vector2f mousePos = app->ui->getMousePosition();

    // Remove selected clip on backspace
    if (selectedClip && backspace && !prevBackspace) {
        // Find the track containing the selected clip - track-aware deletion
        for (auto& t : allTracks) {
            // Only check the selected track
            if (t->getName() != selectedTrack) continue;
            
            const auto& clips = t->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                const auto& clip = clips[i];
                if (clip.startTime == selectedClip->startTime &&
                    clip.duration == selectedClip->duration &&
                    clip.sourceFile == selectedClip->sourceFile) {
                    t->removeClip(static_cast<int>(i));
                    selectedClip = nullptr;
                    forceUpdate = true;
                    std::cout << "Removed selected clip from track '" << t->getName() << "'\n";
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

    // Check if Ctrl is held for scroll speed control
    const bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    // Process each track
    for (const auto& track : allTracks) {
        const std::string rowKey = track->getName() + "_scrollable_row";
        auto rowIt = containers.find(rowKey);
        if (rowIt == containers.end()) continue;
        
        auto* trackRow = rowIt->second;
        auto* scrollableRow = static_cast<ScrollableRow*>(trackRow);

        // Frame-rate independent scroll speed (pixels per second) - disable when Ctrl is held
        constexpr float baseScrollSpeedPerSecond = 1800.0f;
        const float frameRateIndependentScrollSpeed = ctrlHeld ? 0.0f : baseScrollSpeedPerSecond * deltaTime;
        scrollableRow->setScrollSpeed(frameRateIndependentScrollSpeed);
        timelineElement->setScrollSpeed(frameRateIndependentScrollSpeed);
        scrollableRow->setOffset(clampedOffset);

        // Skip track if it's not visible on screen
        const sf::Vector2f trackRowPos = trackRow->getPosition();
        const sf::Vector2f trackRowSize = trackRow->getSize();
        const float timelineBottom = timelineElement->getPosition().y + timelineElement->getSize().y;
        const float timelineTop = timelineElement->getPosition().y;
        
        // Check if track is completely outside the visible timeline area
        if (trackRowPos.y + trackRowSize.y < timelineTop || trackRowPos.y > timelineBottom)
            continue;

        // Generate timeline measures (reuse for performance)
        auto lines = generateTimelineMeasures(beatWidth, clampedOffset, trackRow->getSize(), 4, 4, &app->resources);

        // Clip selection logic - optimized with track-based selection
        const auto& clipsVec = track->getClips();
        const sf::Vector2f localMousePos = mousePos - trackRowPos;
        
        for (size_t i = 0; i < clipsVec.size(); ++i) {
            const auto& ac = clipsVec[i];
            const float clipWidthPixels = ac.duration * pixelsPerSecond;
            const float clipXPosition = (ac.startTime * pixelsPerSecond) + clampedOffset;
            const sf::FloatRect clipRect({clipXPosition, 0.f}, {clipWidthPixels, trackRow->getSize().y});
            
            if (clipRect.contains(localMousePos)) {
                // Normal click (no ctrl): select clip and set selected track
                if (!ctrlPressed && !prevCtrlPressed && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                    selectedClip = const_cast<AudioClip*>(&clipsVec[i]);
                    selectedTrack = track->getName(); // Set the selected track
                    if (!isPlaying)
                        app->setSavedPosition(ac.startTime);
                }
            }
        }

        // Generate clip geometry with track-aware selection
        auto clips = generateClipRects(bpm, beatWidth, clampedOffset, trackRow->getSize(), 
                                     track->getClips(), 0.f, &app->resources, &app->uiState, selectedClip, 
                                     track->getName(), selectedTrack);

        // Batch geometry updates
        std::vector<std::shared_ptr<sf::Drawable>> rowGeometry;
        rowGeometry.reserve(clips.size() + lines.size());
        rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(clips.begin()), std::make_move_iterator(clips.end()));
        rowGeometry.insert(rowGeometry.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
        scrollableRow->setCustomGeometry(rowGeometry);
    }
    prevCtrlPressed = ctrlPressed;
    prevBackspace = backspace;

    // Playhead stays on the timeline as a whole, but align y to first track
    float playheadYOffset = 0.f;
    if (!app->getAllTracks().empty()) {
        const auto& firstTrack = app->getAllTracks()[0];
        std::string firstTrackRowKey = firstTrack->getName() + "_scrollable_row";
        if (containers.count(firstTrackRowKey)) {
            auto* firstTrackRow = containers[firstTrackRowKey];
            playheadYOffset = firstTrackRow->getPosition().y - timelineElement->getPosition().y;
        }
    }

    if (app->getAllTracks().size() > 1) {
        auto playhead = getPlayHead(
            app->getBpm(), 
            100.f * app->uiState.timelineZoomLevel,
            clampedOffset,
            app->getPosition(), 
            sf::Vector2f(4.f, (app->getAllTracks().size() - 1) * (masterTrackElement->getSize().y + 4))
        );
        // Offset playhead y-position
        if (auto playheadRect = std::dynamic_pointer_cast<sf::RectangleShape>(playhead)) {
            sf::Vector2f pos = playheadRect->getPosition();
            pos.y += playheadYOffset;
            playheadRect->setPosition(pos);
        }
        std::vector<std::shared_ptr<sf::Drawable>> timelineGeometry;
        timelineGeometry.push_back(playhead);
        timelineElement->setCustomGeometry(timelineGeometry);
        timelineOffset = clampedOffset;
    }

    if (selectedClip && !app->isPlaying() && app->getPosition() != selectedClip->startTime) {
        // Only set saved position if we have a valid selected track
        if (!selectedTrack.empty()) {
            app->setSavedPosition(selectedClip->startTime);
        }
    }
}

void TimelineComponent::rebuildUI() {
    getColumn("base_timeline_column")->clear();

    parentContainer = app->mainContentRow;
    masterTrackElement = masterTrack();
    
    // Create scrollableColumn with optimal setup
    auto* timelineScrollable = scrollableColumn(
        Modifier(),
        contains{}, "timeline"
    );
    containers["timeline"] = timelineScrollable;
    
    // Pre-allocate container space for better performance
    const auto& allTracks = app->getAllTracks();
    containers.reserve(containers.size() + allTracks.size());
    
    // Optimized track creation loop
    for (const auto& t : allTracks) {
        if (t->getName() == "Master") continue;
        
        auto* trackRowElem = track(t->getName(), Align::TOP | Align::LEFT, t->getVolume(), t->getPan());
        timelineScrollable->addElement(spacer(Modifier().setfixedHeight(4.f)));
        timelineScrollable->addElement(trackRowElem);
        
        // Register the scrollable row for custom drawables - optimized
        if (trackRowElem) {
            const auto& elements = trackRowElem->getElements();
            if (!elements.empty() && elements[0]) {
                const std::string rowKey = t->getName() + "_scrollable_row";
                containers.emplace(rowKey, static_cast<uilo::Container*>(elements[0]));
            }
        }
    }
    
    layout = column(Modifier().setColor(app->resources.activeTheme->middle_color), contains{
        timelineScrollable,
        masterTrackElement
    }, "base_timeline_column");

    parentContainer->addElement(layout);
}

void TimelineComponent::rebuildUIFromEngine() {}

void TimelineComponent::syncSlidersToEngine() {
    // Sync master track slider
    if (app->getMasterTrack() && masterVolumeSlider) {
        float engineVol = app->getMasterTrack()->getVolume();
        float sliderValue = decibelsToFloat(engineVol);
        masterVolumeSlider->setValue(sliderValue);
    }
    
    // Sync regular track sliders
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        auto volumeSlider = trackVolumeSliders.find(track->getName());
        if (volumeSlider != trackVolumeSliders.end() && volumeSlider->second) {
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
    std::cout << "DEBUG: Clearing waveform cache (" << cache.size() << " entries)" << std::endl;
    cache.clear();
}

inline void ensureWaveformIsCached(const AudioClip& clip) {
    if (!clip.sourceFile.existsAsFile()) return;

    auto& cache = getWaveformCache();
    const std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    
    // Use find instead of count for better performance
    if (cache.find(filePath) != cache.end()) return;

    // Thread-safe audio format manager with static instance
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
    
    // Optimized peak calculation with constants
    constexpr float peakResolution = 0.05f; // 50ms per peak
    const int desiredPeaks = std::max(1, static_cast<int>(std::ceil(clip.duration / peakResolution)));
    const long long samplesPerPeak = std::max(1LL, totalSamples / desiredPeaks);

    std::vector<float> peaks;
    peaks.reserve(desiredPeaks);

    // Optimized buffer size and reuse
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

        // Optimized amplitude calculation using SIMD-friendly operations
        float maxAmplitude = 0.0f;
        for (int channel = 0; channel < reader->numChannels; ++channel) {
            const float channelMagnitude = buffer.getMagnitude(channel, 0, numSamplesToRead);
            maxAmplitude = std::max(maxAmplitude, channelMagnitude);
        }
        peaks.emplace_back(maxAmplitude);
    }

    // Use emplace for better performance
    cache.emplace(filePath, std::move(peaks));
}

inline std::vector<std::shared_ptr<sf::Drawable>> generateTimelineMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator = 4,
    unsigned int sigDenominator = 4,
    UIResources* resources = nullptr
) {
    // Early return for invalid parameters
    if (measureWidth <= 0.f || sigNumerator == 0) return {};
    
    std::vector<std::shared_ptr<sf::Drawable>> lines;
    
    // Calculate visible range with small margin for smooth scrolling
    constexpr float margin = 10.f;
    const float visibleWidth = rowSize.x;
    const float startX = -scrollOffset;
    const float endX = startX + visibleWidth;
    
    const int startMeasure = static_cast<int>(std::floor(startX / measureWidth));
    const int endMeasure = static_cast<int>(std::ceil(endX / measureWidth)) + 1;
    
    // Pre-calculate constants for performance
    const float beatWidth = measureWidth / sigNumerator;
    const sf::Color& lineColor = resources->activeTheme->line_color;
    sf::Color transparentLineColor = lineColor;
    transparentLineColor.a = 100;
    
    // Estimate and reserve space for better performance
    const int measureCount = endMeasure - startMeasure + 1;
    const int totalLines = measureCount * sigNumerator; // Rough estimate
    lines.reserve(totalLines);
    
    // Generate measures with optimized inner loop
    for (int measure = startMeasure; measure <= endMeasure; ++measure) {
        const float xPos = std::fma(static_cast<float>(measure), measureWidth, scrollOffset);
        
        // Check visibility with margin
        if (xPos >= -margin && xPos <= visibleWidth + margin) {
            // Main measure line
            auto measureLine = std::make_shared<sf::RectangleShape>();
            measureLine->setSize({2.f, rowSize.y});
            measureLine->setPosition({xPos, 0.f});
            measureLine->setFillColor(lineColor);
            lines.emplace_back(std::move(measureLine));
        }
        
        // Generate beat subdivision lines with optimized loop
        for (unsigned int beat = 1; beat < sigNumerator; ++beat) {
            const float beatX = std::fma(static_cast<float>(beat), beatWidth, xPos);
            
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
    clipRects.reserve(clips.size() * 2); // Pre-allocate for clips + waveforms
    
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    const sf::Color& clipColor = resources->activeTheme->clip_color;
    constexpr sf::Color whiteColor = sf::Color::White;
    constexpr float outlineThickness = 3.f;

    for (const auto& ac : clips) {
        // Generate clip rectangle with optimal allocation
        const float clipWidthPixels = ac.duration * pixelsPerSecond;
        const float clipXPosition = (ac.startTime * pixelsPerSecond) + scrollOffset;

        // Track-aware selection check: only show outline if clip is selected AND we're in the selected track
        const bool isSelected = selectedClip && 
                                currentTrackName == selectedTrackName &&
                                ac.startTime == selectedClip->startTime &&
                                ac.duration == selectedClip->duration &&
                                ac.sourceFile == selectedClip->sourceFile;

        if (isSelected) {
            // Background rectangle (acts as the "inset outline") - only for selected clips
            auto outlineRect = std::make_shared<sf::RectangleShape>();
            outlineRect->setSize({clipWidthPixels, rowSize.y});
            outlineRect->setPosition({clipXPosition, 0.f});
            outlineRect->setFillColor(sf::Color(
                255 - clipColor.r,
                255 - clipColor.g,
                255 - clipColor.b
            )); // Inverted color outline
            clipRects.emplace_back(std::move(outlineRect));

            // Foreground rectangle (the actual clip content) - inset for selected clips
            auto clipRect = std::make_shared<sf::RectangleShape>();
            const float insetThickness = 3.f;
            clipRect->setSize({clipWidthPixels - 2 * insetThickness, rowSize.y - 2 * insetThickness});
            clipRect->setPosition({clipXPosition + insetThickness, insetThickness});
            clipRect->setFillColor(clipColor);
            clipRects.emplace_back(std::move(clipRect));
        } else {
            // Normal clip rectangle (no outline) - for non-selected clips
            auto clipRect = std::make_shared<sf::RectangleShape>();
            clipRect->setSize({clipWidthPixels, rowSize.y});
            clipRect->setPosition({clipXPosition, 0.f});
            clipRect->setFillColor(clipColor);
            clipRects.emplace_back(std::move(clipRect));
        }

        // Generate waveform with move semantics
        auto waveformDrawables = generateWaveformData(
            ac, 
            sf::Vector2f(clipXPosition, 0.f), 
            sf::Vector2f(clipWidthPixels, rowSize.y),
            verticalOffset,
            resources,
            uiState
        );
        
        // Use move iterator for efficient insertion
        clipRects.insert(clipRects.end(), 
                        std::make_move_iterator(waveformDrawables.begin()), 
                        std::make_move_iterator(waveformDrawables.end()));
    }
    
    return clipRects;
}

inline std::shared_ptr<sf::Drawable> getPlayHead(double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize) {
    auto playHeadRect = std::make_shared<sf::RectangleShape>();

    const float xPosition = secondsToXPosition(bpm, beatWidth, seconds);
    
    // Optimized with constexpr and single assignment
    constexpr float playheadWidth = 4.f;
    constexpr sf::Color playheadColor(
        255, 0, 0, 100
    );

    playHeadRect->setSize({playheadWidth, rowSize.y});
    playHeadRect->setPosition({xPosition + scrollOffset, 0.f});
    playHeadRect->setFillColor(playheadColor);
    
    return playHeadRect;
}

inline float getNearestMeasureX(const sf::Vector2f& pos, const std::vector<std::shared_ptr<sf::Drawable>>& lines) {
    if (lines.empty()) return pos.x;
    
    float closestX = 0.0f;
    float minDistance = std::numeric_limits<float>::max();
    
    // Optimized loop with const references and early type checking
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
    // Optimized calculation using std::fma for better precision
    constexpr float secondsPerMinute = 60.0f;
    const float pixelsPerSecond = (beatWidth * static_cast<float>(bpm)) / secondsPerMinute;
    return seconds * pixelsPerSecond;
}

inline float xPosToSeconds(double bpm, float beatWidth, float xPos, float scrollOffset) noexcept {
    // Optimized calculation with constexpr
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

    // Optimized constants
    constexpr int upsample = 5;
    constexpr float waveformScale = 0.9f;
    constexpr float peakThreshold = 0.001f;
    
    const int numPeaks = static_cast<int>(peaks.size());
    const int numSamples = numPeaks * upsample;
    
    sf::Color waveformColorWithAlpha = resources->activeTheme->wave_form_color;
    waveformColorWithAlpha.a = 180;

    // Pre-calculate commonly used values
    const float invNumSamples = 1.0f / numSamples;
    const float invNumPeaksMinusOne = 1.0f / (numPeaks - 1);
    const float lineHeightScale = clipSize.y * waveformScale;
    const float baseLineY = clipPosition.y + clipSize.y * 0.5f + verticalOffset;

    // Use VertexArray for optimal batch rendering (SFML 3.0 feature)
    auto vertexArray = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
    vertexArray->resize(numSamples * 2); // Pre-allocate for efficiency

    // Optimized waveform generation
    size_t vertexIndex = 0;
    for (int i = 0; i < numSamples; ++i) {
        const float t = i * invNumSamples * (numPeaks - 1);
        const int idx = static_cast<int>(t);
        const float frac = t - idx;
        
        // Linear interpolation with bounds checking
        float peakValue = peaks[idx];
        if (idx + 1 < numPeaks) {
            peakValue = std::fma(peaks[idx + 1] - peaks[idx], frac, peaks[idx]);
        }
        
        if (peakValue > peakThreshold) {
            const float lineHeight = peakValue * lineHeightScale;
            const float lineX = std::fma(i * invNumSamples, clipSize.x, clipPosition.x) + 4.0f; // Push 2 pixels to the right
            const float lineYTop = baseLineY - lineHeight * 0.5f;
            const float lineYBottom = baseLineY + lineHeight * 0.5f;
            
            // Add vertices using direct indexing for efficiency
            if (vertexIndex + 1 < vertexArray->getVertexCount()) {
                (*vertexArray)[vertexIndex].position = sf::Vector2f(lineX, lineYTop);
                (*vertexArray)[vertexIndex].color = waveformColorWithAlpha;
                (*vertexArray)[vertexIndex + 1].position = sf::Vector2f(lineX, lineYBottom);
                (*vertexArray)[vertexIndex + 1].color = waveformColorWithAlpha;
                vertexIndex += 2;
            }
        }
    }
    
    // Resize to actual used vertices
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