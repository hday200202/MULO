#pragma once

#include "MULOComponent.hpp"
#include "Track.hpp"
#include <limits>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <algorithm>

#define vector_contains(container, element) \
    (std::find((container).begin(), (container).end(), (element)) != (container).end())

#define map_contains(map, key) \
    ((map).find(key) != (map).end())

struct WaveformLOD {
    std::vector<std::shared_ptr<sf::VertexArray>> lodLevels;
    std::vector<int> samplesPerLine;
    double durationSeconds = 0.0;
    std::atomic<bool> isReady{false};
    
    int selectLOD(double pixelsPerSecond, double sampleRate = 44100.0) const {
        if (lodLevels.empty()) return 0;
        
        double targetLinesPerSecond = pixelsPerSecond * 1.5;
        int bestLOD = 0;
        double bestDiff = std::numeric_limits<double>::max();
        
        for (size_t i = 0; i < samplesPerLine.size(); ++i) {
            double linesPerSecond = sampleRate / samplesPerLine[i];
            double diff = std::abs(linesPerSecond - targetLinesPerSecond);
            
            if (diff < bestDiff) {
                bestDiff = diff;
                bestLOD = static_cast<int>(i);
            }
            
            if (linesPerSecond < targetLinesPerSecond && i > 0)
                break;
        }
        
        return bestLOD;
    }
};

class WaveformClipDrawable : public sf::Drawable, public sf::Transformable {
public:
    WaveformClipDrawable(std::shared_ptr<WaveformLOD> lod,
                         float clipX, float clipY, float clipWidth, float clipHeight,
                         double offset, double duration, double totalDuration,
                         float viewportLeft = -std::numeric_limits<float>::max(),
                         float viewportRight = std::numeric_limits<float>::max())
        : lod(lod), clipX(clipX), clipY(clipY), 
          clipWidth(clipWidth), clipHeight(clipHeight),
          offset(offset), duration(duration), totalDuration(totalDuration),
          viewportLeft(viewportLeft), viewportRight(viewportRight) {}
    
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        if (!lod || !lod->isReady || lod->lodLevels.empty() || duration <= 0) return;
        
        double pixelsPerSecond = clipWidth / duration;
        int lodIndex = lod->selectLOD(pixelsPerSecond);
        
        if (lodIndex < 0 || lodIndex >= static_cast<int>(lod->lodLevels.size())) return;
        
        auto& vertices = lod->lodLevels[lodIndex];
        if (!vertices || vertices->getVertexCount() == 0) return;
        
        float visibleClipLeft = std::max(clipX, viewportLeft);
        float visibleClipRight = std::min(clipX + clipWidth, viewportRight);
        if (visibleClipLeft >= visibleClipRight) return;
        
        double pixelsPerSec = clipWidth / duration;
        double visibleStartTime = offset + (visibleClipLeft - clipX) / pixelsPerSec;
        double visibleEndTime = offset + (visibleClipRight - clipX) / pixelsPerSec;
        visibleStartTime = std::max(visibleStartTime, offset);
        visibleEndTime = std::min(visibleEndTime, offset + duration);
        
        if (visibleStartTime >= visibleEndTime) return;
        
        const size_t vertexCount = vertices->getVertexCount();
        size_t startIndex = 0;
        {
            size_t left = 0, right = vertexCount;
            while (left < right) {
                size_t mid = (left + right) / 2;
                mid = (mid / 2) * 2;
                if (mid >= vertexCount) break;
                double time = (*vertices)[mid].position.x;
                if (time < visibleStartTime) left = mid + 2;
                else right = mid;
            }
            startIndex = left;
        }
        
        // Binary search for visible end vertex
        size_t endIndex = 0;
        {
            size_t left = startIndex, right = vertexCount;
            while (left < right) {
                size_t mid = (left + right) / 2;
                mid = (mid / 2) * 2;
                if (mid >= vertexCount) break;
                double time = (*vertices)[mid].position.x;
                if (time < visibleEndTime) {
                    left = mid + 2;
                } else {
                    right = mid;
                }
            }
            endIndex = left;
        }
        
        if (startIndex >= vertexCount || startIndex >= endIndex) return;
        endIndex = std::min(endIndex, vertexCount);
        
        const size_t numLines = (endIndex - startIndex) / 2;
        if (numLines == 0) return;
        
        sf::VertexArray clippedWaveform(sf::PrimitiveType::TriangleStrip, numLines * 2);
        
        const float centerY = clipY + clipHeight / 2.0f;
        const float scale = clipHeight / 2.0f * 0.9f;
        
        sf::Color waveColor = (*vertices)[startIndex].color;
        waveColor.a = 180;
        
        size_t outIdx = 0;
        for (size_t i = startIndex; i < endIndex && i + 1 < vertexCount; i += 2) {
            double vertexTime = (*vertices)[i].position.x;
            double relativeTime = vertexTime - offset;
            float screenX = clipX + static_cast<float>(relativeTime * pixelsPerSec);
            
            float minAmp = (*vertices)[i].position.y;
            float maxAmp = (*vertices)[i + 1].position.y;
            
            float screenYMin = centerY + minAmp * scale;
            float screenYMax = centerY + maxAmp * scale;
            
            if (outIdx + 1 < clippedWaveform.getVertexCount()) {
                clippedWaveform[outIdx].position = sf::Vector2f(screenX, screenYMin);
                clippedWaveform[outIdx].color = waveColor;
                outIdx++;
                
                clippedWaveform[outIdx].position = sf::Vector2f(screenX, screenYMax);
                clippedWaveform[outIdx].color = waveColor;
                outIdx++;
            }
        }
        
        if (outIdx < clippedWaveform.getVertexCount()) {
            clippedWaveform.resize(outIdx);
        }
        
        if (clippedWaveform.getVertexCount() > 0) {
            target.draw(clippedWaveform, states);
        }
    }
    
private:
    std::shared_ptr<WaveformLOD> lod;
    float clipX, clipY, clipWidth, clipHeight;
    double offset, duration, totalDuration;
    float viewportLeft, viewportRight;  // Screen-space viewport bounds for culling
};

class TimelineComponent : public MULOComponent {
public:
    TimelineComponent() { name = "timeline"; }
    ~TimelineComponent() override {}

    void init() override;
    bool handleEvents() override;
    void update() override;

    MIDIClip* getSelectedMIDIClip() const override;

    struct SelectedMIDIClipInfo {
        bool hasSelection = false;
        double startTime = 0.0;
        double duration = 0.0;
        std::string trackName = "";
    } selectedMIDIClipInfo;

private:
    struct EngineState {
        double position = 0.0;      
        std::unordered_map<std::string, Track*> trackCache;
        
        void rebuildTrackCache(std::vector<std::unique_ptr<Track>>& engineTracks) {
            trackCache.clear();
            for (auto& track : engineTracks)
                trackCache[track->getName()] = track.get();
        }
        
        Track* getTrack(const std::string& trackName) {
            auto it = trackCache.find(trackName);
            if (it != trackCache.end()) return it->second;
            return nullptr;
        }
    };

    struct UIState {
        float xOffset = 0.f;
        float yOffset = 0.f;
        float prevXOffset = 0.f;

        float zoom = 1.f;
        float prevZoom = 1.f;

        float beatWidth = 10.f;

        int waveformRes = 10;

        float laneHeight = 96.f;
        float labelWidth = 256.f;
        
        float lastUiScale = 1.f;

        double leftSidePosSeconds = 0.0;
        double rightSidePosSeconds = 4.0;

        double cursorPosition = 0.0;
        std::string cursorTrackName = "";
        bool showCursor = false;
        
        std::string selectedClipTrack = "";
        double selectedClipStartTime = -1.0;
        bool selectedClipIsMIDI = false;
        
        bool followPlayhead = true;

        bool measureLinesShouldUpdate = false;
        bool didZoom = false;
        
        // Drag and resize state
        enum class ResizeMode { None, Start, End };
        bool isDraggingClip = false;
        bool isResizingClip = false;
        ResizeMode resizeMode = ResizeMode::None;
        
        double dragStartMouseTime = 0.0;
        double dragOffsetWithinClip = 0.0;
        
        double originalClipStartTime = 0.0;
        double originalClipDuration = 0.0;
        size_t resizingClipIndex = 0;

        std::string previousSelectedTrack = "";
        
        // Clipboard state
        std::vector<AudioClip> copiedAudioClips;
        std::vector<MIDIClip> copiedMIDIClips;
        bool hasClipboard = false;
        
        // Initial update counter to force refreshes on startup
        int initialUpdateCount = 0;
    };
    
    struct Input {
        bool leftMousePressed = false;
        bool leftMouseClicked = false;
        bool rightMousePressed = false;
        bool rightMouseClicked = false;
        bool ctrlPressed = false;
        sf::Vector2f mousePosition;
        float scrollDelta = 0.f;
        
        bool prevLeftMousePressed = false;
        bool prevRightMousePressed = false;
        
        // Double-click detection
        sf::Clock doubleClickClock;
        int clickCount = 0;
        bool isDoubleClick = false;
        static constexpr float DOUBLE_CLICK_TIME = 0.250f;
        
        void updateInput(Application* app, Container* layoutBounds);
    };

    EngineState engineState;
    Input input;
    std::vector<std::string> tracksToDelete;

    // UI
    UIState uiState;
    Column* baseColumn = nullptr;
    Row* baseRow = nullptr;
    Column* timelineColumn = nullptr;
    ScrollableColumn* laneScrollable;
    ScrollableColumn* labelScrollable;
    std::unordered_set<std::string> tracksInUI;
    
    struct TrackContainer {
        Column* laneColumn = nullptr;
        Column* labelColumn = nullptr;
        std::vector<Row*> automationLaneRows;
        std::vector<Row*> automationLabelRows;
    };
    std::unordered_map<std::string, TrackContainer> trackContainers;
    
    struct AutomationDragState {
        bool isDragging = false;
        bool isCurveEditing = false;
        bool isInteracting = false;
        sf::Clock interactionClock;
        sf::Vector2f startMousePos;
        float startTime = 0.0f;
        float startValue = 0.0f;
        float startCurve = 0.5f;
        float endValue = 0.0f;
        std::string trackName;
        std::string effectName;
        std::string parameterName;
        std::string laneScrollableId;
        float originalTime = -1.0f;
        float originalValue = -1.0f;
    } automationDragState;
    
    std::unordered_map<std::string, std::shared_ptr<WaveformLOD>> trackWaveforms;  // Maps track name to waveform
    std::unordered_map<std::string, std::shared_ptr<WaveformLOD>> waveformCache;  // Maps file hash to waveform
    std::unordered_map<std::string, std::string> trackToFileHash;  // Maps track name to file hash
    std::unordered_map<std::string, double> trackRefClipDurations;
    std::mutex waveformMutex;  // Protects all waveform maps

    std::shared_ptr<sf::VertexArray> measureLines;
    std::shared_ptr<sf::VertexArray> subMeasureLines;
    std::vector<std::shared_ptr<sf::RectangleShape>> measureBarShapes;
    std::vector<std::shared_ptr<sf::RectangleShape>> automationBarShapes;
    std::shared_ptr<sf::RectangleShape> virtualCursor;
    std::shared_ptr<sf::RectangleShape> playhead;
    sf::Clock cursorBlinkClock;
    
    std::vector<double> gridLinePositions;

    Container* buildUILayout();
    void newTrack(const Track& track);
    void updateAutomationLanes();
    std::tuple<std::string, std::string, float> getCurrentAutomationParameter(Track* track);
    void syncSlidersToEngine();
    void handleAutomationDragOperations();
    std::vector<std::shared_ptr<sf::Drawable>> generateAutomationLine(
        const std::string& trackName,
        const std::string& effectName,
        const std::string& parameter,
        float defaultValue,
        uilo::Row* automationLane
    );
    void updateMeasureLines();
    void updateVirtualCursor();
    void updatePlayhead();
    std::vector<std::shared_ptr<sf::Drawable>> generateClipShapes(const std::string& trackName);
    bool handleScroll();
    bool handleZoom();
    void syncLaneOffsets();
    void handleClipPlacement();
    void handleClipSelection();
    void handleClipDeletion();
    void handleClipResize();
    void handleClipDrag();
    void handleClipCopy();
    void handleClipPaste();
    void handleClipDuplicate();

    // Helper functions
    bool isMouseInAutomationLane(const std::string& trackName, float mouseY);
    void createAutomationLane(const std::string& trackName, const std::string& effectName, 
                              const std::string& parameterName, bool isPotential);
    float calculateBeatWidth(double secondsInView);
    float getEffectiveBeatWidth(float beatWidth);
    double getBeatsPerSecond();
    float getLaneStartX();
    double findNearestGridLine(double seconds);

    void updateEngineState();
    void generateTrackWaveform(const std::string& trackName);
    sf::Texture generateWaveformTexture(const Track& track);

    double xPosToSeconds(const float xPos);
    float secondsToXPos(const double seconds);
};

#include "Application.hpp"

void TimelineComponent::Input::updateInput(Application* app, Container* layoutBounds) {
    prevLeftMousePressed = leftMousePressed;
    prevRightMousePressed = rightMousePressed;
    
    mousePosition = app->ui->getMousePosition();
    scrollDelta = app->ui->getVerticalScrollDelta();
    
    bool mouseInBounds = false;
    if (layoutBounds) {
        sf::FloatRect bounds = layoutBounds->m_bounds.getGlobalBounds();
        mouseInBounds = bounds.contains(mousePosition);
    }
    if (!mouseInBounds) return;
    
    if (mouseInBounds) {
        leftMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        rightMousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    } else {
        leftMousePressed = false;
        rightMousePressed = false;
    }
    
    ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || 
                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    
    leftMouseClicked = leftMousePressed && !prevLeftMousePressed;
    rightMouseClicked = rightMousePressed && !prevRightMousePressed;
    
    // Detect double-click
    isDoubleClick = false;
    if (leftMouseClicked) {
        float elapsed = doubleClickClock.getElapsedTime().asSeconds();
        if (elapsed < DOUBLE_CLICK_TIME && clickCount == 1) {
            isDoubleClick = true;
            clickCount = 0;
        } else {
            clickCount = 1;
            doubleClickClock.restart();
        }
    }
    
    if (doubleClickClock.getElapsedTime().asSeconds() > DOUBLE_CLICK_TIME)
        clickCount = 0;
}

void TimelineComponent::init() {
    parentContainer = app->mainContentRow;
    initialized = false;
    relativeTo = "file_browser";

    layout = buildUILayout();
    labelScrollable->setScrollSpeed(40.f);
    laneScrollable->setScrollSpeed(40.f);
    parentContainer->addElement(layout);

    float viewWidthPixels = laneScrollable->getSize().x;

    if (viewWidthPixels > 0) {
        double bpm = app->getBpm();
        double beatsPerSecond = bpm / 60.0;
        double secondsInView = 4.0;
        float beatsInView = secondsInView * beatsPerSecond;
        uiState.beatWidth = viewWidthPixels / beatsInView;
    } else uiState.beatWidth = 182.25f;  // ~4 seconds
    
    updateMeasureLines();
    updateEngineState();
    
    uiState.lastUiScale = app->ui->getScale();
    uiState.initialUpdateCount = 5; // Force 5 initial updates
    uiState.measureLinesShouldUpdate = true; // Force initial update

    cursorBlinkClock.restart();
    initialized = true;
}

bool TimelineComponent::handleEvents() {
    if (!this->isVisible()) return false;
    input.updateInput(app, laneScrollable);
    
    if (!input.leftMousePressed && !input.rightMousePressed)
        automationDragState.isInteracting = false;
    
    // Unlock scrollables from previous zoom
    if (uiState.didZoom) {
        laneScrollable->unlock();
        labelScrollable->unlock();
        uiState.didZoom = false;
    }

    // No vertical scroll if ctrl is pressed
    if (input.ctrlPressed) {
        laneScrollable->lock();
        labelScrollable->lock();
        uiState.didZoom = true;
    }

    // Handle clip placement on double-click
    if (input.isDoubleClick)
        handleClipPlacement();

    // Handle cursor placement and clip selection on left click
    if (input.leftMouseClicked && !input.isDoubleClick) {
        float mouseY = input.mousePosition.y;
        double clickTimePos = xPosToSeconds(input.mousePosition.x);
        
        for (const auto& trackName : tracksInUI) {
            auto trackLane = app->ui->getRow(trackName + "_lane_scrollable");
            if (!trackLane) continue;
            
            auto bounds = trackLane->m_bounds;
            if (mouseY >= bounds.getPosition().y && mouseY <= bounds.getPosition().y + bounds.getSize().y) {
                // Skip if click is in automation lane area
                if (!isMouseInAutomationLane(trackName, mouseY)) {
                    double nearestGridLine = findNearestGridLine(clickTimePos);
                    uiState.cursorPosition = nearestGridLine;
                    uiState.cursorTrackName = trackName;
                    uiState.showCursor = true;
                    app->setSelectedTrack(trackName);
                    cursorBlinkClock.restart();
                }
                break;
            }
        }
        
        handleClipSelection();
    }

    if (!app->isPlaying())
        app->setSavedPosition(uiState.cursorPosition);
    
    // Handle clip resize and drag (always check, not just on click)
    handleClipResize();
    handleClipDrag();
    
    // Handle cursor placement and clip deletion on right click
    if (input.rightMouseClicked) {
        float mouseY = input.mousePosition.y;
        double clickTimePos = xPosToSeconds(input.mousePosition.x);
        
        for (const auto& trackName : tracksInUI) {
            auto trackLane = app->ui->getRow(trackName + "_lane_scrollable");
            if (!trackLane) continue;
            
            auto bounds = trackLane->m_bounds;
            if (mouseY >= bounds.getPosition().y && mouseY <= bounds.getPosition().y + bounds.getSize().y) {
                // Skip if click is in automation lane area
                if (!isMouseInAutomationLane(trackName, mouseY)) {
                    double nearestGridLine = findNearestGridLine(clickTimePos);
                    uiState.cursorPosition = nearestGridLine;
                    uiState.cursorTrackName = trackName;
                    app->setSelectedTrack(trackName);
                }
                break;
            }
        }
        
        handleClipDeletion();
    }

    bool zoomHandled = handleZoom();
    bool scrollHandled = handleScroll();

    if (app->getSelectedTrack() != uiState.previousSelectedTrack) {
        auto prevLabel = app->ui->getRow(uiState.previousSelectedTrack + "_label");
        if (prevLabel) prevLabel->m_modifier.setColor(app->resources.activeTheme->track_color);
        auto currLabel = app->ui->getRow(app->getSelectedTrack() + "_label");
        if (currLabel) currLabel->m_modifier.setColor(app->resources.activeTheme->clip_color);
        uiState.previousSelectedTrack = app->getSelectedTrack();
    }

    // Handle keyboard shortcuts for clipboard operations
    static bool prevC = false, prevV = false, prevD = false;
    bool c = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::C);
    bool v = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);
    bool d = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
    bool ctrl = input.ctrlPressed;
    
    if (ctrl && c && !prevC) {
        handleClipCopy();
    }
    if (ctrl && v && !prevV) {
        handleClipPaste();
    }
    if (ctrl && d && !prevD) {
        handleClipDuplicate();
    }
    
    prevC = c;
    prevV = v;
    prevD = d;

    return zoomHandled || scrollHandled;
}

bool TimelineComponent::handleZoom() {
    if (!input.ctrlPressed)
        return false;
    
    float scrollDelta = input.scrollDelta;
    if (scrollDelta == 0.f) return false;

    float mouseX = input.mousePosition.x;    
    double targetMouseTimePos = xPosToSeconds(mouseX);
    
    // Zoom by adjusting beatWidth directly (10% change per scroll)
    float zoomFactor = (scrollDelta > 0) ? 1.1f : 0.9f;
    uiState.beatWidth *= zoomFactor;
    
    // Recalculate effective values
    float newEffectiveBeatWidth = getEffectiveBeatWidth(uiState.beatWidth);
    double beatsPerSecond = getBeatsPerSecond();
    float laneStart = getLaneStartX();
    
    // Calculate new xOffset to keep mouse position stable
    float newXOffset = (mouseX - laneStart) - (targetMouseTimePos * beatsPerSecond * newEffectiveBeatWidth);
    
    if (newXOffset > 0) uiState.xOffset = 0;
    else uiState.xOffset = newXOffset;

    syncLaneOffsets();
    
    app->ui->resetScrollDeltas();
    uiState.measureLinesShouldUpdate = true;

    return true;
}

void TimelineComponent::syncLaneOffsets() {
    for (const auto& trackName : tracksInUI) {
        auto trackLaneRow = app->ui->getRow(trackName + "_lane_scrollable");
        if (trackLaneRow) {
            auto trackLane = dynamic_cast<ScrollableRow*>(trackLaneRow);
            if (trackLane) trackLane->setOffset(uiState.xOffset);
        }
        
        // Sync all automation lanes for this track
        if (trackContainers.count(trackName)) {
            for (auto* laneRow : trackContainers[trackName].automationLaneRows) {
                if (laneRow) {
                    // laneRow is the wrapper row, we need to get the scrollable child
                    const auto& elements = laneRow->getElements();
                    if (!elements.empty()) {
                        auto automationLane = dynamic_cast<ScrollableRow*>(elements[0]);
                        if (automationLane) automationLane->setOffset(uiState.xOffset);
                    }
                }
            }
        }
    }
    
    auto masterLaneRow = app->ui->getRow("Master_lane_scrollable");
    if (masterLaneRow) {
        auto masterLane = dynamic_cast<ScrollableRow*>(masterLaneRow);
        if (masterLane) masterLane->setOffset(uiState.xOffset);
    }
}

bool TimelineComponent::handleScroll() {
    // handle all scroll events and update UI
    if (!laneScrollable) return false;
    
    uiState.yOffset = laneScrollable->getOffset();
    
    static float prevXOffset = 0.f;
    static float prevLaneScrollOffset = 0.f;
    static float prevLabelScrollOffset = 0.f;
    
    float laneScrollOffset = laneScrollable->getOffset();
    float labelScrollOffset = labelScrollable->getOffset();

    if (laneScrollOffset != prevLaneScrollOffset) {
        labelScrollable->setOffset(laneScrollOffset);
        prevLaneScrollOffset = laneScrollOffset;
        prevLabelScrollOffset = laneScrollOffset;
    }
    else if (labelScrollOffset != prevLabelScrollOffset) {
        laneScrollable->setOffset(labelScrollOffset);
        prevLabelScrollOffset = labelScrollOffset;
        prevLaneScrollOffset = labelScrollOffset;
    }
    
    float currentXOffset = 0.f;
    bool xOffsetFound = false;
    
    for (const auto& trackName : tracksInUI) {
        auto trackLane = dynamic_cast<ScrollableRow*>(app->ui->getRow(trackName + "_lane_scrollable"));
        if (trackLane) {
            float trackOffset = trackLane->getOffset();
            if (!xOffsetFound) {
                currentXOffset = trackOffset;
                xOffsetFound = true;
            }
            if (trackOffset != prevXOffset) {
                currentXOffset = trackOffset;
                break;
            }
        }
    }
    
    auto masterLane = dynamic_cast<ScrollableRow*>(app->ui->getRow("Master_lane_scrollable"));
    if (masterLane) {
        float masterOffset = masterLane->getOffset();
        if (!xOffsetFound) {
            currentXOffset = masterOffset;
            xOffsetFound = true;
        }
        if (masterOffset != prevXOffset)
            currentXOffset = masterOffset;
    }
    
    if (currentXOffset != prevXOffset) {
        if (currentXOffset > 0.f) currentXOffset = 0.f;
        
        for (const auto& trackName : tracksInUI) {
            auto trackLane = dynamic_cast<ScrollableRow*>(app->ui->getRow(trackName + "_lane_scrollable"));
            if (trackLane) trackLane->setOffset(currentXOffset);
        }
        
        if (masterLane) masterLane->setOffset(currentXOffset);
        
        uiState.xOffset = currentXOffset;
        prevXOffset = currentXOffset;
        uiState.measureLinesShouldUpdate = true;
    }
    
    return false;
}

void TimelineComponent::update() {    
    // Force updates during initial startup phase to ensure proper rendering
    if (uiState.initialUpdateCount > 0) {
        uiState.measureLinesShouldUpdate = true;
        uiState.initialUpdateCount--;
    }
    
    // Generate missing waveforms when playback stops
    static bool wasPlaying = false;
    bool isPlaying = app->isPlaying();
    if (wasPlaying && !isPlaying) {
        // Playback just stopped, generate any missing waveforms
        for (const auto& trackName : tracksInUI) {
            auto track = app->getTrack(trackName);
            if (track && track->getType() == Track::TrackType::Audio) {
                std::lock_guard<std::mutex> lock(waveformMutex);
                if (trackWaveforms.count(trackName) == 0) {
                    // Track doesn't have waveform, generate it (will be done outside lock)
                    generateTrackWaveform(trackName);
                }
            }
        }
    }
    wasPlaying = isPlaying;
    
    // Sync volume sliders bidirectionally with engine
    syncSlidersToEngine();
    
    // Handle automation point dragging
    handleAutomationDragOperations();
    
    // Detect UI scale changes and reinitialize if changed
    float currentScale = app->ui->getScale();
    if (std::abs(currentScale - uiState.lastUiScale) > 0.001f) {
        double viewCenterTime = (uiState.leftSidePosSeconds + uiState.rightSidePosSeconds) / 2.0;
        
        float scaleRatio = uiState.lastUiScale / currentScale;
        uiState.xOffset *= scaleRatio;        
        uiState.lastUiScale = currentScale;
        
        uiState.measureLinesShouldUpdate = true;
        
        // Update measure lines FIRST with new scale
        updateMeasureLines();
        updateVirtualCursor();
        updatePlayhead();
        
        // Rebuild automation lanes with new scale
        static bool prevShowAutomation = app->readConfig<bool>("show_automation", false);
        bool currentShowAutomation = app->readConfig<bool>("show_automation", false);
        if (currentShowAutomation) {
            updateAutomationLanes();
        }
        prevShowAutomation = currentShowAutomation;
        
        // Sync all lane offsets AFTER automation lanes are recreated
        syncLaneOffsets();
        
        // Rebuild clip shapes for all tracks
        for (const auto& trackName : tracksInUI) {
            auto trackLane = app->ui->getRow(trackName + "_lane_scrollable");
            if (trackLane) {
                std::vector<std::shared_ptr<sf::Drawable>> customGeom;
                
                for (auto& bar : measureBarShapes)
                    customGeom.push_back(bar);
                
                auto clipShapes = generateClipShapes(trackName);
                for (auto& clip : clipShapes)
                    customGeom.push_back(clip);
                
                customGeom.push_back(subMeasureLines);
                customGeom.push_back(measureLines);
                
                if (uiState.showCursor && virtualCursor && uiState.cursorTrackName == trackName)
                    customGeom.push_back(virtualCursor);
                
                if (playhead && app->isPlaying())
                    customGeom.push_back(playhead);
                
                trackLane->setCustomGeometry(customGeom);
            }
        }
        
        auto masterLane = app->ui->getRow("Master_lane_scrollable");
        if (masterLane) {
            std::vector<std::shared_ptr<sf::Drawable>> customGeom;
            
            for (auto& bar : measureBarShapes)
                customGeom.push_back(bar);
            
            customGeom.push_back(subMeasureLines);
            customGeom.push_back(measureLines);
            
            if (uiState.showCursor && virtualCursor && uiState.cursorTrackName == "Master")
                customGeom.push_back(virtualCursor);
            
            if (playhead && app->isPlaying())
                customGeom.push_back(playhead);
            
            masterLane->setCustomGeometry(customGeom);
        }
        
        uiState.measureLinesShouldUpdate = false;
    }
    
    bool tracksChanged = false;
    
    // If any tracks to delete, remove them from the UI
    for (auto& trackName : tracksToDelete) {
        if (trackContainers.count(trackName)) {
            auto& container = trackContainers[trackName];
            
            // remove automation lanes
            for (auto* laneRow : container.automationLaneRows)
                if (laneRow && container.laneColumn)
                    container.laneColumn->removeElement(laneRow);
            for (auto* labelRow : container.automationLabelRows)
                if (labelRow && container.labelColumn)
                    container.labelColumn->removeElement(labelRow);

            container.automationLaneRows.clear();
            container.automationLabelRows.clear();
            
            // remove columns
            if (container.laneColumn)
                laneScrollable->removeElement(container.laneColumn);
            if (container.labelColumn)
                labelScrollable->removeElement(container.labelColumn);
            
            trackContainers.erase(trackName);
        } else {
            // Fallback for Master or tracks without containers
            auto laneRow = app->ui->getRow(trackName + "_lane_row");
            auto labelRow = app->ui->getRow(trackName + "_label_row");
            if (laneRow) laneScrollable->removeElement(laneRow);
            if (labelRow) labelScrollable->removeElement(labelRow);
        }
        
        app->removeTrack(trackName);
        tracksInUI.erase(trackName);
        tracksChanged = true;
    }
    
    // to prevent audio thread from accessing deleted track data
    if (tracksChanged && !tracksToDelete.empty())
        updateEngineState();

    // If any new track is added, add it to the UI
    for (auto& t : app->getAllTracks()) {
        std::string trackName = t->getName();
        
        if (!vector_contains(tracksToDelete, trackName) && !tracksInUI.count(trackName)) {
            newTrack(*t.get());
            tracksInUI.insert(trackName);
            app->ui->getTextBox(trackName + "_text_box")->setText(trackName);
            tracksChanged = true;
        }
    }

    tracksToDelete.clear();

    // Update automation lane visibility only when needed
    static bool prevShowAutomation = app->readConfig<bool>("show_automation", false);
    bool currentShowAutomation = app->readConfig<bool>("show_automation", false);
    
    // Track potential automation changes for each track
    static std::unordered_map<std::string, std::pair<std::string, std::string>> prevPotentialAutomation;
    bool potentialAutomationChanged = false;
    
    for (const auto& trackName : tracksInUI) {
        auto track = app->getTrack(trackName);
        if (track && track->hasPotentialAutomation()) {
            const auto& currentPotential = track->getPotentialAutomation();
            if (prevPotentialAutomation[trackName] != currentPotential) {
                prevPotentialAutomation[trackName] = currentPotential;
                potentialAutomationChanged = true;
            }
        }
    }
    
    if (tracksChanged || prevShowAutomation != currentShowAutomation || potentialAutomationChanged) {
        updateAutomationLanes();
        prevShowAutomation = currentShowAutomation;
        // Force visual update when automation visibility changes or tracks change
        uiState.measureLinesShouldUpdate = true;
    }

    if (uiState.xOffset != uiState.prevXOffset || uiState.zoom != uiState.prevZoom) {
        uiState.measureLinesShouldUpdate = true;
        uiState.prevXOffset = uiState.xOffset;
        uiState.prevZoom = uiState.zoom;
    }

    // If zoom or scroll, the measure lines should be updated
    if (uiState.measureLinesShouldUpdate || uiState.showCursor) {
        if (uiState.measureLinesShouldUpdate) updateMeasureLines();        
        if (uiState.showCursor) updateVirtualCursor();
        updatePlayhead();
        
        for (const auto& trackName : tracksInUI) {
            auto trackLane = app->ui->getRow(trackName + "_lane_scrollable");
            if (trackLane) {
                std::vector<std::shared_ptr<sf::Drawable>> customGeom;
                
                for (auto& bar : measureBarShapes)
                    customGeom.push_back(bar);
                
                auto clipShapes = generateClipShapes(trackName);
                for (auto& clip : clipShapes)
                    customGeom.push_back(clip);
                
                customGeom.push_back(subMeasureLines);
                customGeom.push_back(measureLines);
                
                if (uiState.showCursor && virtualCursor && uiState.cursorTrackName == trackName)
                    customGeom.push_back(virtualCursor);
                
                if (playhead && app->isPlaying())
                    customGeom.push_back(playhead);
                
                trackLane->setCustomGeometry(customGeom);
            }
            
            // Automation lane drawing is now handled in the continuous update section below
            // (lines ~937-997) which properly iterates all automation lanes
        }
        
        auto masterLane = app->ui->getRow("Master_lane_scrollable");
        if (masterLane) {
            std::vector<std::shared_ptr<sf::Drawable>> customGeom;
            
            for (auto& bar : measureBarShapes)
                customGeom.push_back(bar);
            
            customGeom.push_back(subMeasureLines);
            customGeom.push_back(measureLines);
            
            if (uiState.showCursor && virtualCursor && uiState.cursorTrackName == "Master")
                customGeom.push_back(virtualCursor);
            
            if (playhead && app->isPlaying())
                customGeom.push_back(playhead);
            
            masterLane->setCustomGeometry(customGeom);
        }
        
        uiState.measureLinesShouldUpdate = false;
    }
    
    // Update automation lanes continuously
    for (const auto& trackName : tracksInUI) {
        if (trackContainers.count(trackName) && !trackContainers[trackName].automationLaneRows.empty()) {
            auto track = app->getTrack(trackName);
            if (!track) continue;
            
            // Get all automated parameters
            const auto& automatedParams = track->getAutomatedParameters();
            
            // Update locked lanes (parameters with automation points)
            for (const auto& [effectName, paramName] : automatedParams) {
                std::string laneId = trackName + "_" + effectName + "_" + paramName + "_automation_lane_scrollable";
                
                auto automationLane = app->ui->getRow(laneId);
                if (automationLane) {
                    float currentValue = track->getCurrentParameterValue(effectName, paramName);
                    
                    std::vector<std::shared_ptr<sf::Drawable>> customGeom;
                    
                    for (auto& bar : automationBarShapes)
                        customGeom.push_back(bar);
                    
                    auto automationDrawables = generateAutomationLine(trackName, effectName, paramName, currentValue, automationLane);
                    for (auto& drawable : automationDrawables)
                        customGeom.push_back(drawable);
                    
                    customGeom.push_back(subMeasureLines);
                    customGeom.push_back(measureLines);
                    
                    if (playhead && app->isPlaying())
                        customGeom.push_back(playhead);
                    
                    automationLane->setCustomGeometry(customGeom);
                }
            }
            
            // Update potential lane if it exists
            if (track->hasPotentialAutomation()) {
                const auto& potentialAuto = track->getPotentialAutomation();
                std::string potentialLaneId = trackName + "_potential_automation_lane_scrollable";
                
                auto potentialLane = app->ui->getRow(potentialLaneId);
                if (potentialLane) {
                    float currentValue = track->getCurrentParameterValue(potentialAuto.first, potentialAuto.second);
                    
                    std::vector<std::shared_ptr<sf::Drawable>> customGeom;
                    
                    for (auto& bar : automationBarShapes)
                        customGeom.push_back(bar);
                    
                    auto automationDrawables = generateAutomationLine(trackName, potentialAuto.first, potentialAuto.second, currentValue, potentialLane);
                    for (auto& drawable : automationDrawables)
                        customGeom.push_back(drawable);
                    
                    customGeom.push_back(subMeasureLines);
                    customGeom.push_back(measureLines);
                    
                    if (playhead && app->isPlaying())
                        customGeom.push_back(playhead);
                    
                    potentialLane->setCustomGeometry(customGeom);
                }
            }
        }
    }
}

Container* TimelineComponent::buildUILayout() {
    laneScrollable = scrollableColumn(
        Modifier()
            .setColor(app->resources.activeTheme->middle_color)
            .setHeight(1.f)
            .align(Align::LEFT | Align::TOP),
        contains{}
    );

    labelScrollable = scrollableColumn(
        Modifier()
            .setColor(app->resources.activeTheme->middle_color)
            .setfixedWidth(uiState.labelWidth)
            .setHeight(1.f)
            .align(Align::LEFT | Align::TOP),
        contains{}
    );

    baseRow = row(Modifier().setColor(app->resources.activeTheme->middle_color),
    contains{laneScrollable, labelScrollable});

    timelineColumn = column(
        Modifier()
            .setColor(app->resources.activeTheme->middle_color)
            .align(Align::RIGHT | Align::BOTTOM),
        contains{baseRow}, "timeline_column");

    baseColumn = column(Modifier(),
    contains{timelineColumn});

    return baseColumn;
}

void TimelineComponent::newTrack(const Track& track) {
    std::string trackName = track.getName();
    
    auto newTrackLane = scrollableRow(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(uiState.laneHeight)
            .align(Align::TOP),
        contains{},
        trackName + "_lane_scrollable"
    );
    
    if (uiState.xOffset != 0.f) {
        newTrackLane->setOffset(uiState.xOffset);
    }

    newTrackLane->setScrollSpeed(40.f);

    auto newTrackLabel = row(
        Modifier()
            .setColor(app->resources.activeTheme->track_color)
            .setfixedHeight(uiState.laneHeight)
            .setfixedWidth(uiState.labelWidth)
            .align(Align::TOP),
    contains{
        spacer(Modifier().setfixedWidth(16.f)),

        column(
            Modifier().setWidth(0.2f),
        contains{
            !(trackName == "Master") ?
            button(
                Modifier()
                    .setfixedWidth(18.f)
                    .setfixedHeight(18.f)
                    .setColor(app->resources.activeTheme->mute_color)
                    .align(Align::CENTER_X | Align::CENTER_Y)
                    .onLClick([this, trackName](){ 
                        tracksToDelete.push_back(trackName);
                    }),
                ButtonStyle::Pill,
                "",
                "",
                sf::Color::Transparent,
                trackName + "_delete_button"
            ) : button()
        }),

        textBox(
            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(48.f).setfixedWidth(128.f),
            TBStyle::Default,
            app->resources.dejavuSansFont,
            "",
            app->resources.activeTheme->primary_text_color,
            sf::Color::Transparent,
            trackName + "_text_box"
        ),

        column(
            Modifier().setWidth(0.2f).align(Align::RIGHT),
        contains{
            slider(
                Modifier().setfixedWidth(16).setHeight(0.8f).align(Align::CENTER_Y | Align::CENTER_Y),
                app->resources.activeTheme->slider_knob_color,
                app->resources.activeTheme->slider_bar_color,
                SliderOrientation::Vertical,
                0.75f,
                trackName + "_label_volume_slider"
            )
        }),
        spacer(Modifier().setfixedWidth(16.f).align(Align::RIGHT)),
    }, trackName + "_label");

    if (trackName == "Master") {
        if (measureLines) {
            std::vector<std::shared_ptr<sf::Drawable>> customGeom;
            for (auto& bar : measureBarShapes) {
                customGeom.push_back(bar);
            }
            customGeom.push_back(subMeasureLines);
            customGeom.push_back(measureLines);
            newTrackLane->setCustomGeometry(customGeom);
        }
        
        timelineColumn->addElement(
            row(Modifier().setfixedHeight(uiState.laneHeight),
            contains{newTrackLane, newTrackLabel}, "Master_lane_row")
        );
        return;
    }

    // Create main track row
    auto laneRow = row(Modifier().setfixedHeight(uiState.laneHeight + 4),
    contains{newTrackLane}, trackName + "_lane_row");
    
    auto labelRow = row(Modifier().setfixedHeight(uiState.laneHeight + 4),
    contains{newTrackLabel}, trackName + "_label_row");
    
    // Wrap track rows in columns (automation lanes will be added here later)
    float trackHeight = uiState.laneHeight + 4;
    auto laneColumn = column(
        Modifier().setfixedHeight(trackHeight),
    contains{laneRow}, trackName + "_lane_column");
    
    auto labelColumn = column(
        Modifier().setfixedHeight(trackHeight),
    contains{labelRow}, trackName + "_label_column");
    
    // Store references in container map
    TrackContainer container;
    container.laneColumn = laneColumn;
    container.labelColumn = labelColumn;
    trackContainers[trackName] = container;
    
    // Add columns to scrollables
    laneScrollable->addElement(laneColumn);
    labelScrollable->addElement(labelColumn);
    
    if (track.getType() == Track::TrackType::Audio)
        generateTrackWaveform(trackName);
}

void TimelineComponent::createAutomationLane(const std::string& trackName, const std::string& effectName, 
                                             const std::string& parameterName, bool isPotential) {
    auto& container = trackContainers[trackName];
    auto track = app->getTrack(trackName);
    if (!track) return;
    
    std::string laneId = isPotential ? 
        (trackName + "_potential_automation_lane_scrollable") :
        (trackName + "_" + effectName + "_" + parameterName + "_automation_lane_scrollable");
    
    float currentValue = track->getCurrentParameterValue(effectName, parameterName);
    std::string displayName = effectName.empty() ? parameterName : effectName + " - " + parameterName;
    if (isPotential) displayName = "[+] " + displayName;
    
    // Create automation lane scrollable
    auto automationLaneScrollable = scrollableRow(
        Modifier()
            .setColor(app->resources.activeTheme->automation_lane_color)
            .setfixedHeight(uiState.laneHeight * 0.75f)
            .align(Align::TOP),
        contains{},
        laneId
    );
    
    if (uiState.xOffset != 0.f) {
        automationLaneScrollable->setOffset(uiState.xOffset);
    }
    automationLaneScrollable->setScrollSpeed(40.f);
    
    // Add click handlers for automation point interaction
    auto handleAutomationLeftClick = [this, trackName, effectName, parameterName, laneId]() {
        if (!app->getWindow().hasFocus()) return;
        
        automationDragState.isInteracting = true;
        automationDragState.interactionClock.restart();
        
        auto automationLaneScrollable = app->ui->getRow(laneId);
        if (!automationLaneScrollable) return;
        
        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        sf::Vector2f localMousePos = globalMousePos - automationLaneScrollable->getPosition();
        sf::Vector2f rowSize = automationLaneScrollable->getSize();
        
        Track* track = app->getTrack(trackName);
        if (!track) return;
        
        // Calculate mouse position in timeline coordinates
        float mouseValue = 1.0f - (localMousePos.y / rowSize.y);
        mouseValue = std::max(0.0f, std::min(1.0f, mouseValue));
        
        float mouseTimeInSecs = ((localMousePos.x - uiState.xOffset) / uiState.beatWidth) * 60.0f / app->getBpm();
        
        mouseTimeInSecs = findNearestGridLine(mouseTimeInSecs);
        mouseTimeInSecs = std::max(0.0f, mouseTimeInSecs);
        
        const auto* points = track->getAutomationPoints(effectName, parameterName);
        bool startedDrag = false;
        
        if (points) {
            constexpr float clickTolerance = 20.0f;
            
            auto valueToY = [rowSize](float value) { return rowSize.y * (1.0f - value); };
            auto timeToX = [this](float time) {
                return (time / (60.0f / app->getBpm())) * uiState.beatWidth + uiState.xOffset;
            };
            
            // Check for Ctrl+drag curve editing first
            bool ctrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || 
                            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
            
            if (ctrlHeld && points->size() > 1) {
                // Sort points for curve editing
                std::vector<Track::AutomationPoint> sortedPoints;
                for (const auto& point : *points)
                    if (point.time >= 0.0) sortedPoints.push_back(point);

                std::sort(sortedPoints.begin(), sortedPoints.end(),
                    [](const Track::AutomationPoint& a, const Track::AutomationPoint& b) {
                        return a.time < b.time;
                    });
                
                // Find points surrounding mouse position
                const Track::AutomationPoint* startPoint = nullptr;
                const Track::AutomationPoint* endPoint = nullptr;
                
                for (size_t i = 0; i < sortedPoints.size() - 1; ++i) {
                    if (mouseTimeInSecs >= sortedPoints[i].time && 
                        mouseTimeInSecs <= sortedPoints[i + 1].time) {
                        startPoint = &sortedPoints[i];
                        endPoint = &sortedPoints[i + 1];
                        break;
                    }
                }
                
                if (startPoint && endPoint) {
                    automationDragState.isDragging = true;
                    automationDragState.isCurveEditing = true;
                    automationDragState.startMousePos = globalMousePos;
                    automationDragState.startTime = startPoint->time;
                    automationDragState.startValue = startPoint->value;
                    automationDragState.startCurve = startPoint->curve;
                    automationDragState.endValue = endPoint->value;
                    automationDragState.originalTime = startPoint->time;
                    automationDragState.originalValue = startPoint->value;
                    automationDragState.trackName = trackName;
                    automationDragState.effectName = effectName;
                    automationDragState.parameterName = parameterName;
                    automationDragState.laneScrollableId = laneId;
                    startedDrag = true;
                }
            }
            
            // Check for point dragging
            if (!startedDrag) {
                float closestDistance = std::numeric_limits<float>::max();
                const Track::AutomationPoint* closestPoint = nullptr;
                
                for (const auto& point : *points) {
                    float pointX = timeToX(point.time);
                    float pointY = valueToY(point.value);
                    
                    float deltaX = localMousePos.x - pointX;
                    float deltaY = localMousePos.y - pointY;
                    float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                    
                    if (distance <= clickTolerance && distance < closestDistance) {
                        closestDistance = distance;
                        closestPoint = &point;
                    }
                }
                
                if (closestPoint) {
                    automationDragState.isDragging = true;
                    automationDragState.isCurveEditing = false;
                    automationDragState.startMousePos = globalMousePos;
                    automationDragState.startTime = closestPoint->time;
                    automationDragState.startValue = closestPoint->value;
                    automationDragState.startCurve = closestPoint->curve;
                    automationDragState.originalTime = closestPoint->time;
                    automationDragState.originalValue = closestPoint->value;
                    automationDragState.trackName = trackName;
                    automationDragState.effectName = effectName;
                    automationDragState.parameterName = parameterName;
                    automationDragState.laneScrollableId = laneId;
                    startedDrag = true;
                }
            }
            
            // Add new point if no drag started and Ctrl not held
            if (!startedDrag && !ctrlHeld) {
                Track::AutomationPoint point;
                point.time = mouseTimeInSecs;
                point.value = mouseValue;
                point.curve = 0.5f;
                
                track->addAutomationPoint(effectName, parameterName, point);
                bool isPotentialLane = (laneId.find("_potential_automation_lane") != std::string::npos);
                
                if (isPotentialLane) updateAutomationLanes();
                
                // Update lane ID to the new locked lane (not the potential lane anymore)
                std::string newLaneId = trackName + "_" + effectName + "_" + parameterName + "_automation_lane_scrollable";
                
                // Start dragging the new point
                automationDragState.isDragging = true;
                automationDragState.isCurveEditing = false;
                automationDragState.startMousePos = globalMousePos;
                automationDragState.startTime = point.time;
                automationDragState.startValue = point.value;
                automationDragState.startCurve = point.curve;
                automationDragState.originalTime = point.time;
                automationDragState.originalValue = point.value;
                automationDragState.trackName = trackName;
                automationDragState.effectName = effectName;
                automationDragState.parameterName = parameterName;
                automationDragState.laneScrollableId = newLaneId;
            }
        }    
    };
    
    auto handleAutomationRightClick = [this, trackName, effectName, parameterName, laneId]() {
        if (!app->getWindow().hasFocus()) return;
        
        automationDragState.isInteracting = true;
        automationDragState.interactionClock.restart();
        
        auto automationLaneScrollable = app->ui->getRow(laneId);
        if (!automationLaneScrollable) return;
        
        sf::Vector2f globalMousePos = app->ui->getMousePosition();
        sf::Vector2f localMousePos = globalMousePos - automationLaneScrollable->getPosition();
        sf::Vector2f rowSize = automationLaneScrollable->getSize();
        
        Track* track = app->getTrack(trackName);
        if (!track) return;
        
        const auto* points = track->getAutomationPoints(effectName, parameterName);
        if (points) {
            constexpr float xTolerance = 30.0f;
            constexpr float yTolerance = 20.0f;
            
            auto valueToY = [rowSize](float value) { return rowSize.y * (1.0f - value); };
            auto timeToX = [this](float time) {
                return (time / (60.0f / app->getBpm())) * uiState.beatWidth + uiState.xOffset;
            };
            
            float closestDistance = std::numeric_limits<float>::max();
            float targetTime = -1.0f;
            float targetValue = -1.0f;
            
            for (const auto& point : *points) {
                float pointX = timeToX(point.time);
                float pointY = valueToY(point.value);
                
                float xDiff = std::abs(localMousePos.x - pointX);
                float yDiff = std::abs(localMousePos.y - pointY);
                
                if (xDiff <= xTolerance && yDiff <= yTolerance) {
                    float distance = std::sqrt(xDiff * xDiff + yDiff * yDiff);
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        targetTime = point.time;
                        targetValue = point.value;
                    }
                }
            }
            
            if (targetTime >= 0.0f && targetValue >= 0.0f) {
                track->removeAutomationPointPrecise(effectName, parameterName, targetTime, targetValue);
                const auto* remainingPoints = track->getAutomationPoints(effectName, parameterName);
                bool onlyInitPointRemains = true;
                if (remainingPoints) {
                    for (const auto& point : *remainingPoints) {
                        if (point.time >= 0.0f) {
                            onlyInitPointRemains = false;
                            break;
                        }
                    }
                }
                
                if (onlyInitPointRemains) {
                    updateAutomationLanes();
                }
            }
        }
    };
    
    automationLaneScrollable->m_modifier.onLClick(handleAutomationLeftClick);
    automationLaneScrollable->m_modifier.onRClick(handleAutomationRightClick);
    
    // Add measure lines to automation lane
    if (measureLines) {
        std::vector<std::shared_ptr<sf::Drawable>> customGeom;
        for (auto& bar : automationBarShapes) {
            customGeom.push_back(bar);
        }
        
        auto automationDrawables = generateAutomationLine(trackName, effectName, parameterName, currentValue, automationLaneScrollable);
        for (auto& drawable : automationDrawables)
            customGeom.push_back(drawable);
        
        customGeom.push_back(subMeasureLines);
        customGeom.push_back(measureLines);
        automationLaneScrollable->setCustomGeometry(customGeom);
    }
    
    auto automationLaneRow = row(
        Modifier().setfixedHeight(uiState.laneHeight * 0.75f + 4),
    contains{automationLaneScrollable}, laneId + "_row");
    
    // Create automation label with delete button for locked lanes
    uilo::Row* automationLabel = nullptr;
    
    if (!isPotential) {
        // Locked lane with delete button
        auto deleteButton = button(
            Modifier()
                .setfixedWidth(16.f)
                .setfixedHeight(16.f)
                .setColor(app->resources.activeTheme->mute_color)
                .align(Align::CENTER_Y),
            ButtonStyle::Pill,
            "",
            "",
            sf::Color::Transparent,
            laneId + "_delete_button"
        );
        
        deleteButton->m_modifier.onLClick([this, trackName, effectName, parameterName]() {
            auto track = app->getTrack(trackName);
            if (track) {
                const auto* points = track->getAutomationPoints(effectName, parameterName);
                if (points) {
                    std::vector<std::pair<float, float>> pointsToRemove;
                    for (const auto& point : *points)
                        if (point.time >= 0.0f)
                            pointsToRemove.push_back({point.time, point.value});
                    
                    for (const auto& [time, value] : pointsToRemove)
                        track->removeAutomationPointPrecise(effectName, parameterName, time, value);
                    
                    updateAutomationLanes();
                }
            }
        });
        
        automationLabel = row(
            Modifier()
                .setColor(app->resources.activeTheme->automation_label_color)
                .setfixedHeight(uiState.laneHeight * 0.75f)
                .setfixedWidth(uiState.labelWidth)
                .align(Align::TOP),
        contains{
            spacer(Modifier().setfixedWidth(8.f)),
            deleteButton,
            spacer(Modifier().setfixedWidth(8.f)),
            text(
                Modifier()
                    .setColor(sf::Color::White)  // Use white for visibility
                    .setfixedHeight(18.f)
                    .align(Align::LEFT | Align::CENTER_Y),
                displayName,
                app->resources.dejavuSansFont,
                laneId + "_label_text"
            ),
        }, laneId + "_label_row");
    } else {
        // Potential lane without delete button
        automationLabel = row(
            Modifier()
                .setColor(app->resources.activeTheme->automation_label_color)
                .setfixedHeight(uiState.laneHeight * 0.75f)
                .setfixedWidth(uiState.labelWidth)
                .align(Align::TOP),
        contains{
            spacer(Modifier().setfixedWidth(16.f)),
            text(
                Modifier()
                    .setColor(sf::Color::White)  // Use white for visibility
                    .setfixedHeight(18.f)
                    .align(Align::LEFT | Align::CENTER_Y),
                displayName,
                app->resources.dejavuSansFont,
                laneId + "_label_text"
            ),
        }, laneId + "_label_row");
    }
    
    // Add automation rows to the track columns
    container.laneColumn->addElement(automationLaneRow);
    container.labelColumn->addElement(automationLabel);
    
    // Store references
    container.automationLaneRows.push_back(automationLaneRow);
    container.automationLabelRows.push_back(automationLabel);
}

void TimelineComponent::updateAutomationLanes() {
    bool showAutomation = app->readConfig<bool>("show_automation", false);
    
    for (auto& [trackName, container] : trackContainers) {
        // Get the track to check its automation parameter
        auto track = app->getTrack(trackName);
        if (!track) continue;
        
        if (showAutomation) {
            // Clear ALL existing automation lanes first (so they get recreated with current scale)
            for (auto* laneRow : container.automationLaneRows) {
                container.laneColumn->removeElement(laneRow);
            }
            for (auto* labelRow : container.automationLabelRows) {
                container.labelColumn->removeElement(labelRow);
            }
            container.automationLaneRows.clear();
            container.automationLabelRows.clear();
            
            // Get all automated parameters (parameters with points)
            const auto& automatedParams = track->getAutomatedParameters();
            bool hasPotentialAutomation = track->hasPotentialAutomation();
            std::pair<std::string, std::string> potentialAuto;
            if (hasPotentialAutomation) {
                potentialAuto = track->getPotentialAutomation();
            }
            
            // Create locked lanes for all automated parameters
            for (const auto& [effectName, parameterName] : automatedParams) {
                createAutomationLane(trackName, effectName, parameterName, false);
            }
            
            // Create potential lane if it's not already automated
            if (hasPotentialAutomation) {
                bool alreadyAutomated = false;
                for (const auto& [effectName, parameterName] : automatedParams) {
                    if (effectName == potentialAuto.first && parameterName == potentialAuto.second) {
                        alreadyAutomated = true;
                        break;
                    }
                }
                
                if (!alreadyAutomated) {
                    createAutomationLane(trackName, potentialAuto.first, potentialAuto.second, true);
                }
            }
            
            // Update column heights
            float totalHeight = uiState.laneHeight + 4; // Track lane height
            totalHeight += container.automationLaneRows.size() * (uiState.laneHeight * 0.75f + 4);
            container.laneColumn->m_modifier.setfixedHeight(totalHeight);
            container.labelColumn->m_modifier.setfixedHeight(totalHeight);
            
        } else if (!showAutomation && !container.automationLaneRows.empty()) {
            // Remove automation lanes
            for (auto* laneRow : container.automationLaneRows) {
                container.laneColumn->removeElement(laneRow);
            }
            for (auto* labelRow : container.automationLabelRows) {
                container.labelColumn->removeElement(labelRow);
            }
            
            // Resize the column back to just the track
            float trackHeight = uiState.laneHeight + 4;
            container.laneColumn->m_modifier.setfixedHeight(trackHeight);
            container.labelColumn->m_modifier.setfixedHeight(trackHeight);
            
            // Clear the vectors
            container.automationLaneRows.clear();
            container.automationLabelRows.clear();
        }
    }
}

std::tuple<std::string, std::string, float> TimelineComponent::getCurrentAutomationParameter(Track* track) {
    if (!track) return {"", "Volume", track ? track->getVolume() : 1.0f};
    
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

void TimelineComponent::syncSlidersToEngine() {
    auto activeDragSlider = app->ui->m_activeDragSlider;

    for (auto& track : tracksInUI) {
        if (activeDragSlider) {
            if (activeDragSlider->m_name == track + "_label_volume_slider") {
                double flToDb = floatToDecibels(activeDragSlider->getValue());
                app->getTrack(track)->setVolume(flToDb);
                activeDragSlider = nullptr;
                continue;
            }
        }
        
        float dbToFL = decibelsToFloat(app->getTrack(track)->getVolume());
        app->ui->getSlider(track + "_label_volume_slider")->setValue(dbToFL);
    }
}

void TimelineComponent::updateMeasureLines() {
    float viewWidthPixels = laneScrollable->getSize().x;
    double bpm = app->getBpm();
    double beatsPerSecond = bpm / 60.0;    
    
    // Calculate effective beat width from current settings
    float effectiveBeatWidth = uiState.beatWidth * uiState.zoom * app->ui->getScale();
    
    // Calculate the current view range based on xOffset and effectiveBeatWidth
    if (effectiveBeatWidth > 0.01f && beatsPerSecond > 0.0) {
        double startTimeSeconds = (-uiState.xOffset) / effectiveBeatWidth / beatsPerSecond;
        double endTimeSeconds = startTimeSeconds + (viewWidthPixels / effectiveBeatWidth / beatsPerSecond);
        
        uiState.leftSidePosSeconds = startTimeSeconds;
        uiState.rightSidePosSeconds = endTimeSeconds;
    }
    
    double secondsInView = uiState.rightSidePosSeconds - uiState.leftSidePosSeconds;
    if (secondsInView <= 0.0) secondsInView = 4.0;
    
    float beatsInView = secondsInView * beatsPerSecond;
    
    auto timeSignature = app->getTimeSignature();
    int numerator = timeSignature.first;
    int beatsPerMeasure = numerator;
    float measureWidth = uiState.beatWidth * beatsPerMeasure;
    float subDivisionWidth = uiState.beatWidth / numerator;
    
    measureBarShapes.clear();
    gridLinePositions.clear();
    measureLines = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
    subMeasureLines = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
    
    // Create darker bar color (every other measure)
    sf::Color barColor = app->resources.activeTheme->track_row_color;
    barColor.r = static_cast<uint8_t>(barColor.r * 0.95f);
    barColor.g = static_cast<uint8_t>(barColor.g * 0.95f);
    barColor.b = static_cast<uint8_t>(barColor.b * 0.95f);
    
    sf::Color lineColor = app->resources.activeTheme->line_color;
    sf::Color subLineColor = lineColor;
    subLineColor.a = static_cast<uint8_t>(lineColor.a * 0.4f);
    
    float scrollableHeight = laneScrollable->m_bounds.getSize().y;
    
    int startBeat = static_cast<int>(std::floor(-uiState.xOffset / uiState.beatWidth)) - 2;
    int totalLines = (int)beatsInView + 25;
    int startMeasure = startBeat / beatsPerMeasure;
    int totalMeasures = (totalLines / beatsPerMeasure) + 2;
    
    // Create measure bar rectangles for every other measure (for tracks)
    for (int i = startMeasure; i < startMeasure + totalMeasures; ++i) {
        if (i % 2 == 0) {
            float xPos = (i * measureWidth) + uiState.xOffset;
            
            auto barShape = std::make_shared<sf::RectangleShape>(sf::Vector2f(measureWidth, scrollableHeight));
            barShape->setPosition(sf::Vector2f(xPos, 0.f));
            barShape->setFillColor(barColor);
            measureBarShapes.push_back(barShape);
        }
    }
    
    // Create darker zebra stripes for automation lanes
    automationBarShapes.clear();
    sf::Color automationBarColor = app->resources.activeTheme->automation_lane_color;
    automationBarColor.r = static_cast<uint8_t>(automationBarColor.r * 0.85f);
    automationBarColor.g = static_cast<uint8_t>(automationBarColor.g * 0.85f);
    automationBarColor.b = static_cast<uint8_t>(automationBarColor.b * 0.85f);
    
    // Use a fixed height for automation bars - they'll be clipped to lane bounds
    float automationBarHeight = uiState.laneHeight * 0.75f;
    
    for (int i = startMeasure; i < startMeasure + totalMeasures; ++i) {
        if (i % 2 == 0) {
            float xPos = (i * measureWidth) + uiState.xOffset;
            
            auto barShape = std::make_shared<sf::RectangleShape>(sf::Vector2f(measureWidth, automationBarHeight));
            barShape->setPosition(sf::Vector2f(xPos, 0.f));
            barShape->setFillColor(automationBarColor);
            automationBarShapes.push_back(barShape);
        }
    }
    
    // Adaptive grid based on zoom level
    if (uiState.beatWidth < 15.f) {
        for (int i = startMeasure; i < startMeasure + totalMeasures; ++i) {
            float xPos = (i * measureWidth) + uiState.xOffset;
            double timePos = (i * beatsPerMeasure) / beatsPerSecond;
            
            sf::Vertex topVertex;
            topVertex.position = sf::Vector2f(xPos, 0.f);
            topVertex.color = lineColor;
            
            sf::Vertex bottomVertex;
            bottomVertex.position = sf::Vector2f(xPos, scrollableHeight);
            bottomVertex.color = lineColor;
            
            measureLines->append(topVertex);
            measureLines->append(bottomVertex);
            gridLinePositions.push_back(timePos);
        }
    } else if (uiState.beatWidth >= 15.f && uiState.beatWidth < 60.f) {
        for (int i = startMeasure; i < startMeasure + totalMeasures; ++i) {
            float xPos = (i * measureWidth) + uiState.xOffset;
            double timePos = (i * beatsPerMeasure) / beatsPerSecond;
            
            sf::Vertex topVertex;
            topVertex.position = sf::Vector2f(xPos, 0.f);
            topVertex.color = lineColor;
            
            sf::Vertex bottomVertex;
            bottomVertex.position = sf::Vector2f(xPos, scrollableHeight);
            bottomVertex.color = lineColor;
            
            measureLines->append(topVertex);
            measureLines->append(bottomVertex);
            gridLinePositions.push_back(timePos);
            
            // Draw beat subdivisions within this measure
            for (int j = 1; j < beatsPerMeasure; ++j) {
                float beatXPos = xPos + (j * uiState.beatWidth);
                double beatTimePos = (i * beatsPerMeasure + j) / beatsPerSecond;
                
                sf::Vertex beatTopVertex;
                beatTopVertex.position = sf::Vector2f(beatXPos, 0.f);
                beatTopVertex.color = subLineColor;
                
                sf::Vertex beatBottomVertex;
                beatBottomVertex.position = sf::Vector2f(beatXPos, scrollableHeight);
                beatBottomVertex.color = subLineColor;
                
                subMeasureLines->append(beatTopVertex);
                subMeasureLines->append(beatBottomVertex);
                gridLinePositions.push_back(beatTimePos);
            }
        }
    } else {
        int subdivisionsPerBeat = numerator;
        
        // Progressively increase subdivisions based on beatWidth
        if (uiState.beatWidth >= 300.f) subdivisionsPerBeat = 8;
        if (uiState.beatWidth >= 600.f) subdivisionsPerBeat = 16;
        if (uiState.beatWidth >= 1200.f) subdivisionsPerBeat = 32;
        if (uiState.beatWidth >= 2400.f) subdivisionsPerBeat = 64;
        
        float actualSubDivisionWidth = uiState.beatWidth / subdivisionsPerBeat;
        
        // Draw all beat lines and subdivisions
        for (int i = startBeat; i < startBeat + totalLines; ++i) {
            float xPos = (i * uiState.beatWidth) + uiState.xOffset;
            double timePos = i / beatsPerSecond;
            
            sf::Vertex topVertex;
            topVertex.position = sf::Vector2f(xPos, 0.f);
            topVertex.color = lineColor;
            
            sf::Vertex bottomVertex;
            bottomVertex.position = sf::Vector2f(xPos, scrollableHeight);
            bottomVertex.color = lineColor;
            
            measureLines->append(topVertex);
            measureLines->append(bottomVertex);
            gridLinePositions.push_back(timePos);
            
            // Draw subdivisions
            for (int j = 1; j < subdivisionsPerBeat; ++j) {
                float subXPos = xPos + j * actualSubDivisionWidth;
                double subTimePos = (i + (double)j / subdivisionsPerBeat) / beatsPerSecond;
            
                sf::Vertex subTopVertex;
                subTopVertex.position = sf::Vector2f(subXPos, 0.f);
                subTopVertex.color = subLineColor;
                
                sf::Vertex subBottomVertex;
                subBottomVertex.position = sf::Vector2f(subXPos, scrollableHeight);
                subBottomVertex.color = subLineColor;
                
                subMeasureLines->append(subTopVertex);
                subMeasureLines->append(subBottomVertex);
                gridLinePositions.push_back(subTimePos);
            }
        }
    }
}

void TimelineComponent::handleClipPlacement() {
    if (!uiState.showCursor || uiState.cursorTrackName.empty()) return;
    
    // Skip if cursor is in automation lane area
    if (isMouseInAutomationLane(uiState.cursorTrackName, input.mousePosition.y)) return;
    
    Track* track = app->getTrack(uiState.cursorTrackName);
    if (!track) return;
    
    // Audio Tracks
    if (track->getType() == Track::TrackType::Audio) {
        AudioClip* refClip = track->getReferenceClip();
        if (!refClip) return;
        
        // Check for collision with existing clips
        const auto& existingClips = track->getClips();
        double newEndTime = uiState.cursorPosition + refClip->duration;
        
        for (const auto& existingClip : existingClips) {
            double existingEndTime = existingClip.startTime + existingClip.duration;
            if (!(newEndTime <= existingClip.startTime || uiState.cursorPosition >= existingEndTime)) {
                return;
            }
        }
        
        // Add clip
        AudioClip newClip(refClip->sourceFile, uiState.cursorPosition, 0.0, refClip->duration, 1.0f);
        track->addClip(newClip);
        
        updateEngineState();
        uiState.measureLinesShouldUpdate = true;
    }
    
    // MIDI Clips
    else if (track->getType() == Track::TrackType::MIDI) {
        double beatDuration = 60.0 / app->getBpm();
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        
        // Check for collision with existing MIDI clips
        const auto& existingMIDIClips = midiTrack->getMIDIClips();
        double newEndTime = uiState.cursorPosition + beatDuration;
        
        for (const auto& existingClip : existingMIDIClips) {
            double existingEndTime = existingClip.startTime + existingClip.duration;
            if (!(newEndTime <= existingClip.startTime || uiState.cursorPosition >= existingEndTime))
                return;
        }
        
        // Add clip
        MIDIClip newMIDIClip(uiState.cursorPosition, beatDuration, 1, 1.0f);
        midiTrack->addMIDIClip(newMIDIClip);
        
        updateEngineState();        
        uiState.measureLinesShouldUpdate = true;
    }
}

void TimelineComponent::handleClipSelection() {
    uiState.selectedClipTrack = "";
    uiState.selectedClipStartTime = -1.0;
    uiState.selectedClipIsMIDI = false;
    
    if (!uiState.showCursor || uiState.cursorTrackName.empty()) return;    
    if (automationDragState.isInteracting) return;
    
    // Skip if cursor is in automation lane area
    if (isMouseInAutomationLane(uiState.cursorTrackName, input.mousePosition.y)) return;
    
    Track* track = app->getTrack(uiState.cursorTrackName);
    if (!track) return;
    
    if (track->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        for (const auto& clip : midiTrack->getMIDIClips()) {
            if (uiState.cursorPosition >= clip.startTime && uiState.cursorPosition <= (clip.startTime + clip.duration)) {
                uiState.selectedClipTrack = uiState.cursorTrackName;
                uiState.selectedClipStartTime = clip.startTime;
                uiState.selectedClipIsMIDI = true;
                
                selectedMIDIClipInfo.hasSelection = true;
                selectedMIDIClipInfo.startTime = clip.startTime;
                selectedMIDIClipInfo.duration = clip.duration;
                selectedMIDIClipInfo.trackName = uiState.cursorTrackName;
                return;
            }
        }
    } else {
        for (const auto& clip : track->getClips()) {
            if (uiState.cursorPosition >= clip.startTime && uiState.cursorPosition <= (clip.startTime + clip.duration)) {
                uiState.selectedClipTrack = uiState.cursorTrackName;
                uiState.selectedClipStartTime = clip.startTime;
                uiState.selectedClipIsMIDI = false;
                
                selectedMIDIClipInfo.hasSelection = false;
                return;
            }
        }
    }
}

void TimelineComponent::handleClipDeletion() {
    if (!uiState.showCursor || uiState.cursorTrackName.empty()) return;    
    if (automationDragState.isInteracting) return;
    
    // Skip if cursor is in automation lane area
    if (isMouseInAutomationLane(uiState.cursorTrackName, input.mousePosition.y)) return;
    
    Track* track = app->getTrack(uiState.cursorTrackName);
    if (!track) return;
    
    if (track->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        const auto& midiClips = midiTrack->getMIDIClips();
        
        for (size_t i = 0; i < midiClips.size(); ++i) {
            const auto& clip = midiClips[i];
            if (uiState.cursorPosition >= clip.startTime && uiState.cursorPosition <= (clip.startTime + clip.duration)) {
                midiTrack->removeMIDIClip(i);
                
                if (uiState.selectedClipTrack == uiState.cursorTrackName && 
                    std::abs(uiState.selectedClipStartTime - clip.startTime) < 0.001) {
                    uiState.selectedClipTrack = "";
                    uiState.selectedClipStartTime = -1.0;
                }
                
                updateEngineState();
                uiState.measureLinesShouldUpdate = true;
                return;
            }
        }
    } else {
        const auto& clips = track->getClips();
        
        for (size_t i = 0; i < clips.size(); ++i) {
            const auto& clip = clips[i];
            if (uiState.cursorPosition >= clip.startTime && uiState.cursorPosition <= (clip.startTime + clip.duration)) {
                track->removeClip(i);
                
                if (uiState.selectedClipTrack == uiState.cursorTrackName && 
                    std::abs(uiState.selectedClipStartTime - clip.startTime) < 0.001) {
                    uiState.selectedClipTrack = "";
                    uiState.selectedClipStartTime = -1.0;
                }
                
                updateEngineState();
                uiState.measureLinesShouldUpdate = true;
                return;
            }
        }
    }
}

void TimelineComponent::handleClipResize() {
    if (automationDragState.isInteracting) return;
    
    // If resizing, handle it continuously
    if (uiState.isResizingClip) {
        if (!input.leftMousePressed) {
            uiState.isResizingClip = false;
            uiState.resizeMode = UIState::ResizeMode::None;
            updateEngineState();
            return;
        }
        
        Track* track = app->getTrack(uiState.selectedClipTrack);
        if (!track) return;
        
        // Get current mouse time and snap it to grid ONLY during resize
        double currentMouseTime = xPosToSeconds(input.mousePosition.x);
        double snappedMouseTime = findNearestGridLine(currentMouseTime);
        
        if (uiState.selectedClipIsMIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            auto clips = midiTrack->getMIDIClips();
            
            // Use the stored index directly
            if (uiState.resizingClipIndex < clips.size()) {
                MIDIClip* clip = midiTrack->getMIDIClip(uiState.resizingClipIndex);
                if (clip) {
                    if (uiState.resizeMode == UIState::ResizeMode::Start) {
                        // Resize from start
                        double fixedEndTime = uiState.originalClipStartTime + uiState.originalClipDuration;
                        double newStartTime = snappedMouseTime;
                        newStartTime = std::max(0.0, newStartTime);
                        
                        double newDuration = fixedEndTime - newStartTime;
                        if (newDuration > 0.1) {
                            clip->startTime = newStartTime;
                            clip->duration = newDuration;
                            uiState.selectedClipStartTime = newStartTime;
                            
                            // Set virtual cursor to the start of the clip
                            uiState.cursorPosition = newStartTime;
                            uiState.showCursor = true;
                        }
                    } else if (uiState.resizeMode == UIState::ResizeMode::End) {
                        // Resize from end
                        double newDuration = snappedMouseTime - uiState.originalClipStartTime;
                        if (newDuration > 0.1) {
                            clip->duration = newDuration;
                            
                            // Set virtual cursor to the end of the clip
                            uiState.cursorPosition = uiState.originalClipStartTime + newDuration;
                            uiState.showCursor = true;
                        }
                    }
                    
                    uiState.measureLinesShouldUpdate = true;
                }
            }
        } else {
            auto clips = track->getClips();
            
            // Use the stored index directly
            if (uiState.resizingClipIndex < clips.size()) {
                AudioClip* clip = track->getClip(uiState.resizingClipIndex);
                if (clip) {
                    if (uiState.resizeMode == UIState::ResizeMode::Start) {
                        // Resize from start - adjust offset too
                        double fixedEndTime = uiState.originalClipStartTime + uiState.originalClipDuration;
                        double newStartTime = snappedMouseTime;
                        newStartTime = std::max(0.0, newStartTime);
                        
                        double newDuration = fixedEndTime - newStartTime;
                        if (newDuration > 0.1) {
                            double timeDiff = newStartTime - clip->startTime;
                            double newOffset = clip->offset + timeDiff;
                            
                            // Don't allow offset to go below 0
                            if (newOffset >= 0.0) {
                                clip->startTime = newStartTime;
                                clip->duration = newDuration;
                                clip->offset = newOffset;
                                uiState.selectedClipStartTime = newStartTime;
                                
                                // Set virtual cursor to the start of the clip
                                uiState.cursorPosition = newStartTime;
                                uiState.showCursor = true;
                            }
                        }
                    } else if (uiState.resizeMode == UIState::ResizeMode::End) {
                        // Resize from end
                        double newDuration = snappedMouseTime - uiState.originalClipStartTime;
                        if (newDuration > 0.1) {
                            clip->duration = newDuration;
                            
                            // Set virtual cursor to the end of the clip
                            uiState.cursorPosition = uiState.originalClipStartTime + newDuration;
                            uiState.showCursor = true;
                        }
                    }
                    
                    uiState.measureLinesShouldUpdate = true;
                }
            }
        }
        return;
    }
    
    // Not resizing - check ALL clips (not just selected) for resize initiation
    const float edgeThreshold = 10.0f; // pixels
    const float pixelsPerSecond = uiState.beatWidth * uiState.zoom;
    const double edgeThresholdSeconds = edgeThreshold / pixelsPerSecond;
    
    double currentMouseTime = xPosToSeconds(input.mousePosition.x);
    float mouseY = input.mousePosition.y;
    
    // Check all tracks for clips under the mouse
    for (const auto& trackName : tracksInUI) {
        auto trackLane = app->ui->getRow(trackName + "_lane_scrollable");
        if (!trackLane) continue;
        
        auto bounds = trackLane->m_bounds;
        if (mouseY < bounds.getPosition().y || mouseY > bounds.getPosition().y + bounds.getSize().y) {
            continue; // Mouse not on this track
        }
        
        // Skip if mouse is in automation lane area
        if (isMouseInAutomationLane(trackName, mouseY)) {
            continue;
        }
        
        Track* track = app->getTrack(trackName);
        if (!track) continue;
        
        // Check MIDI clips
        if (track->getType() == Track::TrackType::MIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            const auto& clips = midiTrack->getMIDIClips();
            
            for (size_t i = 0; i < clips.size(); ++i) {
                double clipEnd = clips[i].startTime + clips[i].duration;
                
                // Check if near start edge
                if (std::abs(currentMouseTime - clips[i].startTime) <= edgeThresholdSeconds) {
                    // Set horizontal resize cursor
                    try {
                        sf::Cursor cursor(sf::Cursor::Type::SizeHorizontal);
                        const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(cursor);
                    } catch (...) {}
                    
                    // Start resize if mouse pressed
                    if (input.leftMousePressed && !input.prevLeftMousePressed) {
                        // Select this clip
                        uiState.selectedClipTrack = trackName;
                        uiState.selectedClipStartTime = clips[i].startTime;
                        uiState.selectedClipIsMIDI = true;
                        selectedMIDIClipInfo.hasSelection = true;
                        selectedMIDIClipInfo.startTime = clips[i].startTime;
                        selectedMIDIClipInfo.duration = clips[i].duration;
                        selectedMIDIClipInfo.trackName = trackName;
                        
                        // Start resize
                        uiState.isResizingClip = true;
                        uiState.resizeMode = UIState::ResizeMode::Start;
                        uiState.resizingClipIndex = i;
                        uiState.originalClipStartTime = clips[i].startTime;
                        uiState.originalClipDuration = clips[i].duration;
                        uiState.dragStartMouseTime = currentMouseTime;
                    }
                    return;
                }
                // Check if near end edge
                else if (std::abs(currentMouseTime - clipEnd) <= edgeThresholdSeconds) {
                    // Set horizontal resize cursor
                    try {
                        sf::Cursor cursor(sf::Cursor::Type::SizeHorizontal);
                        const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(cursor);
                    } catch (...) {}
                    
                    // Start resize if mouse pressed
                    if (input.leftMousePressed && !input.prevLeftMousePressed) {
                        // Select this clip
                        uiState.selectedClipTrack = trackName;
                        uiState.selectedClipStartTime = clips[i].startTime;
                        uiState.selectedClipIsMIDI = true;
                        selectedMIDIClipInfo.hasSelection = true;
                        selectedMIDIClipInfo.startTime = clips[i].startTime;
                        selectedMIDIClipInfo.duration = clips[i].duration;
                        selectedMIDIClipInfo.trackName = trackName;
                        
                        // Start resize
                        uiState.isResizingClip = true;
                        uiState.resizeMode = UIState::ResizeMode::End;
                        uiState.resizingClipIndex = i;
                        uiState.originalClipStartTime = clips[i].startTime;
                        uiState.originalClipDuration = clips[i].duration;
                        uiState.dragStartMouseTime = currentMouseTime;
                    }
                    return;
                }
            }
        } else {
            // Check Audio clips
            const auto& clips = track->getClips();
            
            for (size_t i = 0; i < clips.size(); ++i) {
                double clipEnd = clips[i].startTime + clips[i].duration;
                
                // Check if near start edge
                if (std::abs(currentMouseTime - clips[i].startTime) <= edgeThresholdSeconds) {
                    // Set horizontal resize cursor
                    try {
                        sf::Cursor cursor(sf::Cursor::Type::SizeHorizontal);
                        const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(cursor);
                    } catch (...) {}
                    
                    // Start resize if mouse pressed
                    if (input.leftMousePressed && !input.prevLeftMousePressed) {
                        // Select this clip
                        uiState.selectedClipTrack = trackName;
                        uiState.selectedClipStartTime = clips[i].startTime;
                        uiState.selectedClipIsMIDI = false;
                        selectedMIDIClipInfo.hasSelection = false;
                        
                        // Start resize
                        uiState.isResizingClip = true;
                        uiState.resizeMode = UIState::ResizeMode::Start;
                        uiState.resizingClipIndex = i;
                        uiState.originalClipStartTime = clips[i].startTime;
                        uiState.originalClipDuration = clips[i].duration;
                        uiState.dragStartMouseTime = currentMouseTime;
                    }
                    return;
                }
                // Check if near end edge
                else if (std::abs(currentMouseTime - clipEnd) <= edgeThresholdSeconds) {
                    // Set horizontal resize cursor
                    try {
                        sf::Cursor cursor(sf::Cursor::Type::SizeHorizontal);
                        const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(cursor);
                    } catch (...) {}
                    
                    // Start resize if mouse pressed
                    if (input.leftMousePressed && !input.prevLeftMousePressed) {
                        // Select this clip
                        uiState.selectedClipTrack = trackName;
                        uiState.selectedClipStartTime = clips[i].startTime;
                        uiState.selectedClipIsMIDI = false;
                        selectedMIDIClipInfo.hasSelection = false;
                        
                        // Start resize
                        uiState.isResizingClip = true;
                        uiState.resizeMode = UIState::ResizeMode::End;
                        uiState.resizingClipIndex = i;
                        uiState.originalClipStartTime = clips[i].startTime;
                        uiState.originalClipDuration = clips[i].duration;
                        uiState.dragStartMouseTime = currentMouseTime;
                    }
                    return;
                }
            }
        }
    }
    
    // Reset cursor if not near any clip edge
    try {
        sf::Cursor cursor(sf::Cursor::Type::Arrow);
        const_cast<sf::RenderWindow&>(app->getWindow()).setMouseCursor(cursor);
    } catch (...) {}
}

void TimelineComponent::handleClipDrag() {
    if (automationDragState.isInteracting) return;
    
    if (!uiState.isDraggingClip) {
        if (!input.leftMousePressed) return;        
        if (uiState.isResizingClip) return;        
        if (uiState.selectedClipTrack.empty() || uiState.selectedClipStartTime < 0.0) return;
        
        // Skip if mouse is in automation lane area
        if (isMouseInAutomationLane(uiState.selectedClipTrack, input.mousePosition.y)) return;
        
        // Only start if mouse was just pressed (not already dragging from previous frame)
        if (!input.prevLeftMousePressed) {
            Track* track = app->getTrack(uiState.selectedClipTrack);
            if (!track) return;
            
            if (!uiState.showCursor || uiState.cursorTrackName.empty()) return;
            
            // Make sure cursor is within the clip body (not near edges for resize)
            const float edgeThreshold = 10.0f;
            const float pixelsPerSecond = uiState.beatWidth * uiState.zoom;
            const double edgeThresholdSeconds = edgeThreshold / pixelsPerSecond;
            
            if (uiState.selectedClipIsMIDI) {
                MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
                const auto& clips = midiTrack->getMIDIClips();
                
                for (const auto& clip : clips) {
                    if (std::abs(clip.startTime - uiState.selectedClipStartTime) < 0.001) {
                        double clipEnd = clip.startTime + clip.duration;
                        
                        // Check if cursor is within clip but not near edges
                        if (uiState.cursorPosition >= (clip.startTime + edgeThresholdSeconds) &&
                            uiState.cursorPosition <= (clipEnd - edgeThresholdSeconds)) {
                            uiState.isDraggingClip = true;
                            uiState.dragStartMouseTime = uiState.cursorPosition;
                            uiState.dragOffsetWithinClip = uiState.cursorPosition - clip.startTime;
                            uiState.originalClipStartTime = clip.startTime;
                        }
                        break;
                    }
                }
            } else {
                const auto& clips = track->getClips();
                
                for (const auto& clip : clips) {
                    if (std::abs(clip.startTime - uiState.selectedClipStartTime) < 0.001) {
                        double clipEnd = clip.startTime + clip.duration;
                        
                        // Check if cursor is within clip but not near edges
                        if (uiState.cursorPosition >= (clip.startTime + edgeThresholdSeconds) &&
                            uiState.cursorPosition <= (clipEnd - edgeThresholdSeconds)) {
                            uiState.isDraggingClip = true;
                            uiState.dragStartMouseTime = uiState.cursorPosition;
                            uiState.dragOffsetWithinClip = uiState.cursorPosition - clip.startTime;
                            uiState.originalClipStartTime = clip.startTime;
                        }
                        break;
                    }
                }
            }
        }
    } else {
        // Already dragging - update the clip position
        if (!input.leftMousePressed) {
            uiState.isDraggingClip = false;
            updateEngineState();
            return;
        }
        
        // Update cursor position from current mouse position
        double currentMouseTime = xPosToSeconds(input.mousePosition.x);
        
        Track* track = app->getTrack(uiState.selectedClipTrack);
        if (!track) return;
        
        // Calculate new position based on current mouse time
        double newStartTime = currentMouseTime - uiState.dragOffsetWithinClip;
        
        // Snap to grid
        newStartTime = findNearestGridLine(newStartTime);
        newStartTime = std::max(0.0, newStartTime);
        
        if (uiState.selectedClipIsMIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            auto clips = midiTrack->getMIDIClips();
            
            for (size_t i = 0; i < clips.size(); ++i) {
                if (std::abs(clips[i].startTime - uiState.originalClipStartTime) < 0.001) {
                    MIDIClip* clip = midiTrack->getMIDIClip(i);
                    if (!clip) break;
                    
                    clip->startTime = newStartTime;
                    uiState.selectedClipStartTime = newStartTime;
                    uiState.originalClipStartTime = newStartTime;
                    uiState.measureLinesShouldUpdate = true;
                    break;
                }
            }
        } else {
            auto clips = track->getClips();
            
            for (size_t i = 0; i < clips.size(); ++i) {
                if (std::abs(clips[i].startTime - uiState.originalClipStartTime) < 0.001) {
                    AudioClip* clip = track->getClip(i);
                    if (!clip) break;
                    clip->startTime = newStartTime;
                    
                    uiState.selectedClipStartTime = newStartTime;
                    uiState.originalClipStartTime = newStartTime;
                    uiState.measureLinesShouldUpdate = true;
                    break;
                }
            }
        }
    }
}

float TimelineComponent::calculateBeatWidth(double secondsInView) {
    float viewWidthPixels = laneScrollable->getSize().x;
    double bpm = app->getBpm();
    float beatsInView = secondsInView * (bpm / 60.0);
    return viewWidthPixels / beatsInView;
}

float TimelineComponent::getEffectiveBeatWidth(float beatWidth) {
    return beatWidth * uiState.zoom * app->ui->getScale();
}

double TimelineComponent::getBeatsPerSecond() {
    return app->getBpm() / 60.0;
}

float TimelineComponent::getLaneStartX() {
    return laneScrollable->m_bounds.getPosition().x;
}

double TimelineComponent::xPosToSeconds(const float xPos) {
    float effectiveBeatWidth = getEffectiveBeatWidth(uiState.beatWidth);
    double beatsPerSecond = getBeatsPerSecond();
    float laneStart = getLaneStartX();
    float timelineXPos = (xPos - laneStart) - uiState.xOffset;
    double beats = timelineXPos / effectiveBeatWidth;

    return beats / beatsPerSecond;
}

float TimelineComponent::secondsToXPos(const double seconds) {
    float effectiveBeatWidth = getEffectiveBeatWidth(uiState.beatWidth);
    double beatsPerSecond = getBeatsPerSecond();
    double beats = seconds * beatsPerSecond;
    float timelineXPos = beats * effectiveBeatWidth;
    
    return timelineXPos + uiState.xOffset;
}

void TimelineComponent::updateEngineState() {
    engineState.rebuildTrackCache(app->getAllTracks());
    engineState.position = app->getPosition();
}

void TimelineComponent::generateTrackWaveform(const std::string& trackName) {
    Track* track = app->getTrack(trackName);
    if (!track || track->getType() != Track::TrackType::Audio) return;
    
    AudioClip* refClip = track->getReferenceClip();
    if (!refClip || !refClip->sourceFile.existsAsFile()) return;
    
    // Don't generate waveforms while playing to avoid race conditions
    if (app->isPlaying()) return;
    
    // Compute hash of audio file for caching
    std::string audioPath = refClip->sourceFile.getFullPathName().toStdString();
    std::string fileHash = std::to_string(std::hash<std::string>{}(audioPath));
    
    // Check if we already have this waveform cached by file hash
    {
        std::lock_guard<std::mutex> lock(waveformMutex);
        
        // Check if track already has a waveform assigned
        if (trackWaveforms.count(trackName) > 0) {
            auto existingLod = trackWaveforms[trackName];
            if (existingLod && !existingLod->isReady) {
                // Generation already in progress, skip to avoid race condition
                return;
            }
        }
        
        // Check if this file already has a cached waveform
        if (waveformCache.count(fileHash) > 0) {
            auto cachedLod = waveformCache[fileHash];
            if (cachedLod && cachedLod->isReady) {
                // Reuse cached waveform
                trackWaveforms[trackName] = cachedLod;
                trackToFileHash[trackName] = fileHash;
                trackRefClipDurations[trackName] = cachedLod->durationSeconds;
                return;
            }
        }
    }
    
    // Load the audio file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(refClip->sourceFile));
    if (!reader) return;
    
    const double sampleRate = reader->sampleRate;
    const long long totalSamples = reader->lengthInSamples;
    const int numChannels = reader->numChannels;
    const double durationSeconds = static_cast<double>(totalSamples) / sampleRate;
    
    // Store duration for later use
    trackRefClipDurations[trackName] = durationSeconds;
    
    auto lod = std::make_shared<WaveformLOD>();
    lod->durationSeconds = durationSeconds;
    
    lod->samplesPerLine = {
        uiState.waveformRes * 4,
        uiState.waveformRes * 8,
        uiState.waveformRes * 20,
        uiState.waveformRes * 40,
        uiState.waveformRes * 80,
        uiState.waveformRes * 200,
        uiState.waveformRes * 400,
        uiState.waveformRes * 800,
        uiState.waveformRes * 2000,
        uiState.waveformRes * 4000,
        uiState.waveformRes * 8000,
        uiState.waveformRes * 20000,
        uiState.waveformRes * 40000,
        uiState.waveformRes * 200000,
        uiState.waveformRes * 400000
    };
    
    // Reserve space for all LOD levels to prevent reallocation during generation
    lod->lodLevels.reserve(lod->samplesPerLine.size());
    
    sf::Color waveColor = app->resources.activeTheme->wave_form_color;
    
    // Assign to both caches BEFORE starting thread
    {
        std::lock_guard<std::mutex> lock(waveformMutex);
        trackWaveforms[trackName] = lod;
        waveformCache[fileHash] = lod;
        trackToFileHash[trackName] = fileHash;
    }
    
    std::thread([lod, trackName, fileHash, totalSamples, sampleRate, durationSeconds, numChannels, 
                 audioPath, waveColor]() {
        
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(juce::File(audioPath)));
        
        // Build all LOD levels in a temporary vector first
        std::vector<std::shared_ptr<sf::VertexArray>> tempLodLevels;
        tempLodLevels.reserve(lod->samplesPerLine.size());
        
        for (size_t lodIdx = 0; lodIdx < lod->samplesPerLine.size(); ++lodIdx) {
            const int samplesPerLine = lod->samplesPerLine[lodIdx];
            const long long numLines = totalSamples / samplesPerLine;
            
            if (numLines > 5000000) continue;
            
            auto waveform = std::make_shared<sf::VertexArray>(sf::PrimitiveType::Lines);
            waveform->resize(numLines * 2);
            
            const int chunkSize = 8192;
            juce::AudioBuffer<float> buffer(numChannels, chunkSize);
            
            size_t vertexIndex = 0;
            for (long long lineIdx = 0; lineIdx < numLines && vertexIndex < waveform->getVertexCount(); ++lineIdx) {
                long long startSample = lineIdx * samplesPerLine;
                long long endSample = std::min(startSample + samplesPerLine, totalSamples);
                
                if (endSample <= startSample) break;
                
                float minAmp = 0.0f;
                float maxAmp = 0.0f;
                
                for (long long sample = startSample; sample < endSample; sample += chunkSize) {
                    long long samplesToRead = std::min(static_cast<long long>(chunkSize), endSample - sample);
                    reader->read(&buffer, 0, static_cast<int>(samplesToRead), sample, true, true);
                    
                    for (int ch = 0; ch < numChannels; ++ch) {
                        const float* channelData = buffer.getReadPointer(ch);
                        for (int i = 0; i < samplesToRead; ++i) {
                            float amplitude = channelData[i];
                            minAmp = std::min(minAmp, amplitude);
                            maxAmp = std::max(maxAmp, amplitude);
                        }
                    }
                }
                
                double timeSeconds = static_cast<double>(startSample) / sampleRate;
                
                (*waveform)[vertexIndex].position = sf::Vector2f(static_cast<float>(timeSeconds), minAmp);
                (*waveform)[vertexIndex].color = waveColor;
                vertexIndex++;
                
                (*waveform)[vertexIndex].position = sf::Vector2f(static_cast<float>(timeSeconds), maxAmp);
                (*waveform)[vertexIndex].color = waveColor;
                vertexIndex++;
            }
            
            if (vertexIndex < waveform->getVertexCount())
                waveform->resize(vertexIndex);
            
            tempLodLevels.push_back(waveform);
            double linesPerSec = static_cast<double>(vertexIndex / 2) / durationSeconds;
        }
        
        lod->lodLevels = std::move(tempLodLevels);
        lod->isReady = true;        
    }).detach();
}

sf::Texture TimelineComponent::generateWaveformTexture(const Track& track) {
    return sf::Texture();
}

bool TimelineComponent::isMouseInAutomationLane(const std::string& trackName, float mouseY) {
    if (trackContainers.count(trackName) && !trackContainers[trackName].automationLaneRows.empty()) {
        // Check all automation lanes for this track
        for (auto* laneRow : trackContainers[trackName].automationLaneRows) {
            if (laneRow) {
                auto autoBounds = laneRow->m_bounds;
                if (mouseY >= autoBounds.getPosition().y && 
                    mouseY <= autoBounds.getPosition().y + autoBounds.getSize().y) {
                    return true;
                }
            }
        }
    }
    return false;
}

double TimelineComponent::findNearestGridLine(double seconds) {
    if (gridLinePositions.empty()) return seconds;
    
    // Find the closest grid line by comparing distances
    double minDist = std::numeric_limits<double>::max();
    double nearest = seconds;
    
    for (const auto& linePos : gridLinePositions) {
        double dist = std::abs(linePos - seconds);
        if (dist < minDist) {
            minDist = dist;
            nearest = linePos;
        }
    }
    
    return nearest;
}

void TimelineComponent::updateVirtualCursor() {
    if (!uiState.showCursor) return;
    
    if (!virtualCursor) virtualCursor = std::make_shared<sf::RectangleShape>();
    
    float cursorX = secondsToXPos(uiState.cursorPosition);
    float scrollableHeight = laneScrollable->m_bounds.getSize().y;

    sf::Color clipColor = app->resources.activeTheme->clip_color;
    sf::Color inverseColor(255 - clipColor.r, 255 - clipColor.g, 255 - clipColor.b, 200);

    // Cursor Blink
    sf::Color currentColor;
    int blink = cursorBlinkClock.getElapsedTime().asMilliseconds();

    if (blink >= 500) currentColor = sf::Color::Transparent;
    else if (blink < 500) currentColor = inverseColor;
    if (blink >= 1000) cursorBlinkClock.restart();

    virtualCursor->setFillColor(currentColor);
    
    virtualCursor->setSize(sf::Vector2f(4.f, scrollableHeight));
    virtualCursor->setPosition(sf::Vector2f(cursorX, 0.f));
}

void TimelineComponent::updatePlayhead() {
    if (!playhead) playhead = std::make_shared<sf::RectangleShape>();
    
    double playheadPosition = app->getPosition();
    float scrollableHeight = laneScrollable->m_bounds.getSize().y;
    
    playhead->setSize(sf::Vector2f(4.f, scrollableHeight));
    playhead->setFillColor(app->resources.activeTheme->mute_color);
    
    if (uiState.followPlayhead && app->isPlaying()) {
        float viewWidthPixels = laneScrollable->getSize().x;
        float centerX = viewWidthPixels / 2.0f;
        double centerTimeSeconds = uiState.leftSidePosSeconds + (uiState.rightSidePosSeconds - uiState.leftSidePosSeconds) / 2.0;
        
        if (playheadPosition > centerTimeSeconds) {
            double beatsPerSecond = app->getBpm() / 60.0;
            double targetOffset = centerX - (playheadPosition * uiState.beatWidth * uiState.zoom * app->ui->getScale() * beatsPerSecond);
            
            for (const auto& trackName : tracksInUI) {
                auto trackLane = dynamic_cast<ScrollableRow*>(app->ui->getRow(trackName + "_lane_scrollable"));
                if (trackLane) trackLane->setOffset(targetOffset);
            }
            
            auto masterLane = dynamic_cast<ScrollableRow*>(app->ui->getRow("Master_lane_scrollable"));
            if (masterLane) masterLane->setOffset(targetOffset);
            
            uiState.xOffset = targetOffset;
            uiState.measureLinesShouldUpdate = true;
            
            playhead->setPosition(sf::Vector2f(centerX, 0.f));
        } else {
            float playheadX = secondsToXPos(playheadPosition);
            playhead->setPosition(sf::Vector2f(playheadX, 0.f));
        }
    } else {
        float playheadX = secondsToXPos(playheadPosition);
        playhead->setPosition(sf::Vector2f(playheadX, 0.f));
    }
}

std::vector<std::shared_ptr<sf::Drawable>> TimelineComponent::generateClipShapes(const std::string& trackName) {
    std::vector<std::shared_ptr<sf::Drawable>> clipShapes;
    
    Track* track = engineState.getTrack(trackName);
    if (!track) return clipShapes;
    
    double viewStartSeconds = uiState.leftSidePosSeconds;
    double viewEndSeconds = uiState.rightSidePosSeconds;

    sf::Color clipColor = app->resources.activeTheme->clip_color;
    sf::Color inverseColor(255 - clipColor.r, 255 - clipColor.g, 255 - clipColor.b, 200);
    
    // Handle MIDI tracks
    if (track->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        const auto& midiClips = midiTrack->getMIDIClips();
        
        for (const auto& clip : midiClips) {
            double clipEndTime = clip.startTime + clip.duration;
            
            if (clipEndTime < viewStartSeconds || clip.startTime > viewEndSeconds)
                continue;
            
            float clipX = secondsToXPos(clip.startTime);
            float clipWidth = secondsToXPos(clipEndTime) - clipX;
            float clipHeight = uiState.laneHeight - 8.f;
            
            auto clipShape = std::make_shared<sf::RectangleShape>(sf::Vector2f(clipWidth, clipHeight));
            clipShape->setPosition(sf::Vector2f(clipX, 4.f));
            clipShape->setFillColor(clipColor);
            
            // Only show outline if this clip is selected
            bool isSelected = (uiState.selectedClipTrack == trackName && 
                             uiState.selectedClipIsMIDI &&
                             std::abs(uiState.selectedClipStartTime - clip.startTime) < 0.001);
            
            if (isSelected) {
                clipShape->setOutlineThickness(4.f);
                clipShape->setOutlineColor(inverseColor);
            } else clipShape->setOutlineThickness(0.f);
            
            clipShapes.push_back(clipShape);
            
            // Draw MIDI notes inside the clip
            if (!clip.midiData.isEmpty()) {
                sf::Color noteColor = app->resources.activeTheme->wave_form_color;
                
                // First pass: find the pitch range of all notes
                int minNote = 127;
                int maxNote = 0;
                std::vector<std::tuple<int, double, double>> noteList; // note number, start time, duration
                
                for (const auto metadata : clip.midiData) {
                    auto message = metadata.getMessage();
                    
                    if (message.isNoteOn()) {
                        int noteNumber = message.getNoteNumber();
                        double noteStartTime = metadata.samplePosition / 44100.0;
                        
                        // Find corresponding note-off
                        double noteDuration = 0.1; // Default
                        for (const auto metadata2 : clip.midiData) {
                            auto message2 = metadata2.getMessage();
                            if (message2.isNoteOff() && message2.getNoteNumber() == noteNumber) {
                                double noteOffTime = metadata2.samplePosition / 44100.0;
                                if (noteOffTime > noteStartTime) {
                                    noteDuration = noteOffTime - noteStartTime;
                                    break;
                                }
                            }
                        }
                        
                        noteList.push_back({noteNumber, noteStartTime, noteDuration});
                        
                        if (noteNumber < minNote) minNote = noteNumber;
                        if (noteNumber > maxNote) maxNote = noteNumber;
                    }
                }
                
                // Calculate pitch range with minimum of 14 notes (1 octave + padding)
                int pitchRange = maxNote - minNote + 1;
                if (pitchRange < 14) {
                    // Center the range around the existing notes
                    int expansion = (14 - pitchRange) / 2;
                    minNote = std::max(0, minNote - expansion - 1);
                    maxNote = std::min(127, maxNote + expansion + 1);
                    pitchRange = maxNote - minNote + 1;
                } else {
                    // Add padding of 1 note above and below
                    minNote = std::max(0, minNote - 1);
                    maxNote = std::min(127, maxNote + 1);
                    pitchRange = maxNote - minNote + 1;
                }
                
                // Second pass: draw all notes with scaled Y positions
                for (const auto& [noteNumber, noteStartTime, noteDuration] : noteList) {
                    // Calculate note position relative to clip start
                    double relativeNoteStart = noteStartTime;
                    double relativeNoteEnd = noteStartTime + noteDuration;
                    
                    // Clip notes to clip boundaries
                    if (relativeNoteEnd < 0 || relativeNoteStart > clip.duration)
                        continue;
                    
                    relativeNoteStart = std::max(0.0, relativeNoteStart);
                    relativeNoteEnd = std::min(clip.duration, relativeNoteEnd);
                    
                    // Calculate screen position
                    float noteX = clipX + (relativeNoteStart / clip.duration) * clipWidth;
                    float noteWidth = ((relativeNoteEnd - relativeNoteStart) / clip.duration) * clipWidth;
                    
                    // Make sure note is at least 1 pixel wide
                    if (noteWidth < 1.f) noteWidth = 1.f;
                    
                    // Calculate Y position based on pitch range
                    // Higher notes at top, lower notes at bottom
                    float noteYRatio = (float)(maxNote - noteNumber) / (float)pitchRange;
                    float noteY = 2.f + clipHeight * noteYRatio * 0.95f; // Leave small margin at top/bottom
                    float noteHeight = std::max(1.5f, clipHeight / (float)pitchRange * 0.9f);
                    
                    auto noteShape = std::make_shared<sf::RectangleShape>(sf::Vector2f(noteWidth, noteHeight));
                    noteShape->setPosition(sf::Vector2f(noteX, noteY));
                    noteShape->setFillColor(noteColor);
                    
                    clipShapes.push_back(noteShape);
                }
            }
        }
    }
    // Handle Audio tracks
    else {
        const auto& clips = track->getClips();
        
        // Get the waveform lod for this track
        std::shared_ptr<WaveformLOD> waveformLOD = nullptr;
        double totalDuration = 0.0;
        
        {
            std::lock_guard<std::mutex> lock(waveformMutex);
            if (trackWaveforms.count(trackName) > 0) {
                waveformLOD = trackWaveforms[trackName];
                if (trackRefClipDurations.count(trackName) > 0) {
                    totalDuration = trackRefClipDurations[trackName];
                }
            }
        }
        
        for (const auto& clip : clips) {
            double clipEndTime = clip.startTime + clip.duration;
            
            if (clipEndTime < viewStartSeconds || clip.startTime > viewEndSeconds)
                continue;
            
            float clipX = secondsToXPos(clip.startTime);
            float clipWidth = secondsToXPos(clipEndTime) - clipX;
            float clipHeight = uiState.laneHeight - 8.f;
            
            // Create the clip background
            auto clipShape = std::make_shared<sf::RectangleShape>(sf::Vector2f(clipWidth, clipHeight));
            clipShape->setPosition(sf::Vector2f(clipX, 4.f));
            clipShape->setFillColor(clipColor);
            
            // Only show outline if this clip is selected
            bool isSelected = (uiState.selectedClipTrack == trackName && 
                             !uiState.selectedClipIsMIDI &&
                             std::abs(uiState.selectedClipStartTime - clip.startTime) < 0.001);
            
            if (isSelected) {
                clipShape->setOutlineThickness(4.f);
                clipShape->setOutlineColor(inverseColor);
            } else clipShape->setOutlineThickness(0.f);
            
            clipShapes.push_back(clipShape);
            
            // Add waveform drawable on top if available
            if (waveformLOD && totalDuration > 0) {
                // Calculate viewport bounds in screen space for culling
                float viewportLeft = secondsToXPos(viewStartSeconds);
                float viewportRight = secondsToXPos(viewEndSeconds);
                
                auto waveformDrawable = std::make_shared<WaveformClipDrawable>(
                    waveformLOD,
                    clipX, 4.f, clipWidth, clipHeight,
                    clip.offset, clip.duration, totalDuration,
                    viewportLeft, viewportRight
                );
                clipShapes.push_back(waveformDrawable);
            }
        }
    }
    
    return clipShapes;
}

inline MIDIClip* TimelineComponent::getSelectedMIDIClip() const {
    if (!selectedMIDIClipInfo.hasSelection) return nullptr;
    
    auto* track = app->getTrack(selectedMIDIClipInfo.trackName);
    if (!track || track->getType() != Track::TrackType::MIDI) return nullptr;
    
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

inline void TimelineComponent::handleClipCopy() {
    uiState.copiedAudioClips.clear();
    uiState.copiedMIDIClips.clear();
    uiState.hasClipboard = false;
    
    // Copy selected AudioClip
    if (!uiState.selectedClipTrack.empty() && uiState.selectedClipStartTime >= 0.0 && !uiState.selectedClipIsMIDI) {
        Track* track = app->getTrack(uiState.selectedClipTrack);
        if (track) {
            const auto& clips = track->getClips();
            for (const auto& clip : clips) {
                if (std::abs(clip.startTime - uiState.selectedClipStartTime) < 0.001) {
                    uiState.copiedAudioClips.push_back(clip);
                    uiState.hasClipboard = true;
                    break;
                }
            }
        }
    }
    
    // Copy selected MIDIClip
    if (selectedMIDIClipInfo.hasSelection) {
        MIDIClip* selectedMIDIClip = getSelectedMIDIClip();
        if (selectedMIDIClip) {
            uiState.copiedMIDIClips.push_back(*selectedMIDIClip);
            uiState.hasClipboard = true;
        }
    }
}

inline void TimelineComponent::handleClipPaste() {
    if (!uiState.hasClipboard) return;
    
    double cursorPosition = uiState.cursorPosition;
    std::string currentTrack = uiState.cursorTrackName;
    
    if (currentTrack.empty()) return;
    
    // Paste AudioClips
    Track* track = app->getTrack(currentTrack);
    if (track && !uiState.copiedAudioClips.empty()) {
        for (const AudioClip& originalClip : uiState.copiedAudioClips) {
            AudioClip newClip = originalClip;
            newClip.startTime = cursorPosition;
            track->addClip(newClip);
        }
        uiState.measureLinesShouldUpdate = true;
    }
    
    // Paste MIDIClips
    if (track && track->getType() == Track::TrackType::MIDI && !uiState.copiedMIDIClips.empty()) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        for (const MIDIClip& originalClip : uiState.copiedMIDIClips) {
            MIDIClip newClip = originalClip.createCopyAtTime(cursorPosition);
            midiTrack->addMIDIClip(newClip);
            
            // Select the pasted clip
            selectedMIDIClipInfo.hasSelection = true;
            selectedMIDIClipInfo.startTime = newClip.startTime;
            selectedMIDIClipInfo.duration = newClip.duration;
            selectedMIDIClipInfo.trackName = currentTrack;
            
            uiState.selectedClipTrack = currentTrack;
            uiState.selectedClipStartTime = newClip.startTime;
            uiState.selectedClipIsMIDI = true;
        }
        uiState.measureLinesShouldUpdate = true;
    }
}

inline void TimelineComponent::handleClipDuplicate() {
    // Duplicate selected AudioClip
    if (!uiState.selectedClipTrack.empty() && uiState.selectedClipStartTime >= 0.0 && !uiState.selectedClipIsMIDI) {
        Track* track = app->getTrack(uiState.selectedClipTrack);
        if (track) {
            const auto& clips = track->getClips();
            for (const auto& clip : clips) {
                if (std::abs(clip.startTime - uiState.selectedClipStartTime) < 0.001) {
                    AudioClip newClip = clip;
                    newClip.startTime = clip.startTime + clip.duration;
                    track->addClip(newClip);
                    
                    // Select the duplicated clip
                    uiState.selectedClipStartTime = newClip.startTime;
                    uiState.measureLinesShouldUpdate = true;
                    break;
                }
            }
        }
    }
    
    // Duplicate selected MIDIClip
    if (selectedMIDIClipInfo.hasSelection) {
        MIDIClip* selectedMIDIClip = getSelectedMIDIClip();
        if (selectedMIDIClip) {
            Track* track = app->getTrack(selectedMIDIClipInfo.trackName);
            if (track && track->getType() == Track::TrackType::MIDI) {
                MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
                double newStartTime = selectedMIDIClip->startTime + selectedMIDIClip->duration;
                MIDIClip newClip = selectedMIDIClip->createCopyAtTime(newStartTime);
                midiTrack->addMIDIClip(newClip);
                
                // Select the duplicated clip
                selectedMIDIClipInfo.startTime = newClip.startTime;
                selectedMIDIClipInfo.duration = newClip.duration;
                uiState.selectedClipStartTime = newClip.startTime;
                uiState.measureLinesShouldUpdate = true;
            }
        }
    }
}

void TimelineComponent::handleAutomationDragOperations() {
    if (!automationDragState.isDragging) return;
    
    if (!app->ui->isMouseDragging()) {
        automationDragState.isDragging = false;
        automationDragState.isInteracting = false;
        automationDragState.originalTime = -1.0f;
        automationDragState.originalValue = -1.0f;
        return;
    }
    
    sf::Vector2f currentMousePos = app->ui->getMousePosition();
    auto* automationRow = app->ui->getRow(automationDragState.laneScrollableId);
    
    if (automationRow) {
        sf::Vector2f localMousePos = currentMousePos - automationRow->getPosition();
        sf::Vector2f rowSize = automationRow->getSize();
        
        if (automationDragState.isCurveEditing) {
            // Curve editing mode
            float deltaY = currentMousePos.y - automationDragState.startMousePos.y;
            bool isAscending = automationDragState.endValue > automationDragState.startValue;
            float curveMultiplier = isAscending ? -1.0f : 1.0f;
            float newCurve = automationDragState.startCurve + (deltaY / 200.0f) * curveMultiplier;
            newCurve = std::max(0.0f, std::min(1.0f, newCurve));
            
            Track* track = app->getTrack(automationDragState.trackName);
            if (track) {
                track->updateAutomationPointCurvePrecise(
                    automationDragState.effectName,
                    automationDragState.parameterName,
                    automationDragState.originalTime,
                    automationDragState.originalValue,
                    newCurve
                );
            }
        } else {
            // Point dragging mode
            float newTime = ((localMousePos.x - uiState.xOffset) / uiState.beatWidth) * 60.0f / app->getBpm();
            float newValue = 1.0f - (localMousePos.y / rowSize.y);
            newValue = std::max(0.0f, std::min(1.0f, newValue));
            newTime = findNearestGridLine(newTime);
            
            if (newTime >= 0.0f) {
                Track* track = app->getTrack(automationDragState.trackName);
                if (track) {
                    const auto* points = track->getAutomationPoints(
                        automationDragState.effectName, 
                        automationDragState.parameterName);
                    
                    if (points) {
                        // Find neighboring points to constrain movement
                        float minTime = 0.0f;
                        float maxTime = std::numeric_limits<float>::max();
                        
                        for (const auto& point : *points) {
                            if (point.time < automationDragState.originalTime && point.time > minTime) {
                                minTime = point.time;
                            }
                            if (point.time > automationDragState.originalTime && point.time < maxTime) {
                                maxTime = point.time;
                            }
                        }
                        
                        constexpr float minGap = 0.00001f;
                        newTime = std::max(minTime + minGap, std::min(newTime, maxTime - minGap));
                    }
                    
                    bool moveSuccess = track->moveAutomationPointPrecise(
                        automationDragState.effectName,
                        automationDragState.parameterName,
                        automationDragState.originalTime,
                        automationDragState.originalValue,
                        newTime,
                        newValue
                    );
                    
                    if (moveSuccess) {
                        automationDragState.originalTime = newTime;
                        automationDragState.originalValue = newValue;
                        automationDragState.startTime = newTime;
                    } else {
                        automationDragState.isDragging = false;
                    }
                }
            }
        }
    }
}

std::vector<std::shared_ptr<sf::Drawable>> TimelineComponent::generateAutomationLine(
    const std::string& trackName,
    const std::string& effectName,
    const std::string& parameter,
    float defaultValue,
    uilo::Row* automationLane
) {
    std::vector<std::shared_ptr<sf::Drawable>> drawables;
    
    Track* track = app->getTrack(trackName);
    if (!track || !automationLane) return drawables;
    
    const auto* automationPoints = track->getAutomationPoints(effectName, parameter);
    
    sf::Vector2f rowSize = automationLane->getSize();
    const float padding = 4.0f;
    const float usableHeight = rowSize.y - 2 * padding;
    
    auto valueToY = [&](float value) -> float {
        float normalizedValue = std::max(0.0f, std::min(value, 1.0f));
        return padding + (1.0f - normalizedValue) * usableHeight;
    };
    
    auto timeToX = [&](double time) -> float {
        return (time / (60.0f / app->getBpm())) * uiState.beatWidth + uiState.xOffset;
    };
    
    if (automationPoints && !automationPoints->empty()) {
        constexpr float pointRadius = 8.0f;
        sf::Color pointColor = app->resources.activeTheme->clip_color;
        sf::Color lineColor = pointColor;
        constexpr float lineHeight = 4.0f;
        
        // Sort points by time
        std::vector<Track::AutomationPoint> sortedPoints;
        for (const auto& point : *automationPoints) {
            if (point.time >= 0.0) {
                sortedPoints.push_back(point);
            }
        }
        std::sort(sortedPoints.begin(), sortedPoints.end(),
            [](const Track::AutomationPoint& a, const Track::AutomationPoint& b) {
                return a.time < b.time;
            });
        
        if (sortedPoints.empty()) {
            // Draw horizontal line at default value
            const float timelineWidth = rowSize.x + std::abs(uiState.xOffset) + 2000.0f;
            const float startX = uiState.xOffset - 1000.0f;
            float yPosition = valueToY(defaultValue);
            
            auto line = std::make_shared<sf::RectangleShape>();
            line->setSize({timelineWidth, lineHeight});
            line->setPosition({startX, yPosition - lineHeight / 2.0f});
            line->setFillColor(lineColor);
            drawables.push_back(line);
        } else if (sortedPoints.size() == 1) {
            // Single point - draw horizontal line at that value
            const auto& point = sortedPoints[0];
            float yPosition = valueToY(point.value);
            const float timelineWidth = rowSize.x + std::abs(uiState.xOffset) + 2000.0f;
            const float startX = uiState.xOffset - 1000.0f;
            
            auto line = std::make_shared<sf::RectangleShape>();
            line->setSize({timelineWidth, lineHeight});
            line->setPosition({startX, yPosition - lineHeight / 2.0f});
            line->setFillColor(lineColor);
            drawables.push_back(line);
            
            // Draw the point
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
            // Multiple points - draw curves between them
            for (size_t i = 0; i < sortedPoints.size() - 1; ++i) {
                const auto& point1 = sortedPoints[i];
                const auto& point2 = sortedPoints[i + 1];
                
                float x1 = timeToX(point1.time);
                float y1 = valueToY(point1.value);
                float x2 = timeToX(point2.time);
                float y2 = valueToY(point2.value);
                
                if ((x1 >= -pointRadius && x1 <= rowSize.x + pointRadius) ||
                    (x2 >= -pointRadius && x2 <= rowSize.x + pointRadius)) {
                    
                    if (std::abs(point1.curve - 0.5f) < 0.001f) {
                        // Linear interpolation
                        float lineLength = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
                        float angleRadians = std::atan2(y2 - y1, x2 - x1);
                        
                        auto line = std::make_shared<sf::RectangleShape>();
                        line->setSize({lineLength, lineHeight});
                        line->setPosition({x1, y1 - lineHeight / 2.0f});
                        line->setRotation(sf::radians(angleRadians));
                        line->setFillColor(lineColor);
                        drawables.push_back(line);
                    } else {
                        // Curved interpolation
                        const int segments = 20;
                        for (int j = 0; j < segments; ++j) {
                            float t1 = static_cast<float>(j) / segments;
                            float t2 = static_cast<float>(j + 1) / segments;
                            
                            auto curvePoint = [&](float t) -> sf::Vector2f {
                                float curve = point1.curve;
                                float adjustedT;
                                
                                if (curve < 0.5f) {
                                    float factor = 50.0f * (0.5f - curve);
                                    adjustedT = std::pow(t, 1.0f + factor);
                                } else {
                                    float factor = 50.0f * (curve - 0.5f);
                                    adjustedT = 1.0f - std::pow(1.0f - t, 1.0f + factor);
                                }
                                
                                float x = x1 + t * (x2 - x1);
                                float y = y1 + adjustedT * (y2 - y1);
                                
                                return sf::Vector2f(x, y);
                            };
                            
                            sf::Vector2f p1 = curvePoint(t1);
                            sf::Vector2f p2 = curvePoint(t2);
                            
                            float lineLength = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
                            float angleRadians = std::atan2(p2.y - p1.y, p2.x - p1.x);
                            
                            auto line = std::make_shared<sf::RectangleShape>();
                            line->setSize({lineLength, lineHeight});
                            line->setPosition({p1.x, p1.y - lineHeight / 2.0f});
                            line->setRotation(sf::radians(angleRadians));
                            line->setFillColor(lineColor);
                            drawables.push_back(line);
                        }
                    }
                }
            }
            
            // Draw extended lines before first and after last point
            if (!sortedPoints.empty()) {
                const float timelineExtension = 2000.0f;
                
                const auto& firstPoint = sortedPoints[0];
                float firstPointX = timeToX(firstPoint.time);
                float firstPointY = valueToY(firstPoint.value);
                
                float startX = uiState.xOffset - timelineExtension;
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
                
                float endX = rowSize.x + std::abs(uiState.xOffset) + timelineExtension;
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
        // No automation points - draw horizontal line at default value
        const float timelineWidth = rowSize.x + std::abs(uiState.xOffset) + 2000.0f;
        const float startX = uiState.xOffset - 1000.0f;
        float yPosition = valueToY(defaultValue);
        
        auto line = std::make_shared<sf::RectangleShape>();
        line->setSize({timelineWidth, 4.0f});
        line->setPosition({startX, yPosition - 2.0f});
        line->setFillColor(app->resources.activeTheme->clip_color);
        drawables.push_back(line);
    }
    
    return drawables;
}

GET_INTERFACE
DECLARE_PLUGIN(TimelineComponent)