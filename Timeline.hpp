#pragma once

#include "MULOComponent.hpp"
#include "AudioClip.hpp"

class Timeline : public MULOComponent {
public:
    Timeline() { name = "timeline"; }
    ~Timeline() override {}
    
    void init() override;
    void update() override;
    bool handleEvents() override;

private:
    float zoom = 1.f;
    double offsetSeconds = 0.0;
    float currentScrollOffset = 0.0f; // Synchronized scroll offset for all rows

    // Scrubber integration
    float lastScrubberPosition = 0.0f;
    bool scrubberPositionChanged = false;
    float expectedTimelineOffset = 0.0f;

    // Cached Geometry
    std::vector<sf::RectangleShape> referenceMeasures;
    
    // Geometry caches for visible elements
    std::unordered_map<std::string, sf::VertexArray> trackWaveforms;
    std::unordered_map<std::string, std::vector<sf::RectangleShape>> trackClips;
    std::unordered_map<std::string, std::vector<sf::CircleShape>> automationPoints;
    std::unordered_map<std::string, sf::VertexArray> automationLines;

    // Engine State Sync
    void syncWithEngine();
    void syncScrollOffsets();
    void updateMeasureLineOffsets();
    void syncWithScrubber();

    // User Input
    void handleInput();
    void handleClipDrag();
    void handleClipResize();
    void handleViewNavigation();

    // View/Camera utilities (the "tilemap camera")
    double xPosToSeconds(float xPos);
    float secondsToXPos(double seconds);
    float getPixelsPerSecond();
    
    // Grid snapping helpers
    double snapToGrid(double timeValue, bool forceSnap = false) const;
    float getNearestMeasureX(const sf::Vector2f& mousePos) const;
    bool isShiftPressed() const;
    
    // Clip placement and management
    void processClipAtPosition(const std::string& trackName, const sf::Vector2f& localMousePos, bool isRightClick = false);
    void rebuildTrackClips(const std::string& trackName);
    Container* createClipContainer(double startTime, double duration, const std::string& clipId);
    float timeToPixels(double timeSeconds) const;
    
    // View culling
    bool isTimeVisible(double timeSeconds);
    float getViewWidth();

    // Geometry generation (only when needed)
    sf::VertexArray generateWaveform(const std::string& trackName);
    std::vector<sf::RectangleShape> generateClips(const std::string& trackName);
    std::vector<sf::CircleShape> generateAutomationPoints(const std::string& trackName, const std::string& paramPath);
    sf::VertexArray generateAutomationLine(const std::string& trackName, const std::string& paramPath);
    std::vector<sf::RectangleShape> generateMeasureLines(float trackHeight, float scrollOffset = 0.0f);
    
    // UI Creation
    Container* buildUILayout();
    Row* newTrack(const std::string& trackName);
    ScrollableRow* newAutomationLane(const std::pair<std::string, std::string>& parameterName, const std::string& parentTrack);
};

#include "Application.hpp"

void Timeline::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;

    relativeTo = "file_browser";
    
    layout = buildUILayout();
    
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void Timeline::update() {
    if (!this->isVisible()) return;
    
    syncWithEngine();
    syncScrollOffsets();
    syncWithScrubber();
    
    // TODO: Update geometry based on current view window
    // TODO: Render visible elements only
}

bool Timeline::handleEvents() {
    // TODO: Handle user input events
    // - Mouse clicks, drags, scrolling
    // - Keyboard shortcuts
    // - Return true if event was handled
    return false;
}

void Timeline::syncWithEngine() {
    if (!layout) return;
    
    // Get timeline scrollable column
    auto* baseColumn = static_cast<Column*>(layout);
    if (!baseColumn || baseColumn->getElements().empty()) return;
    
    auto* timelineColumn = static_cast<Column*>(baseColumn->getElements()[0]);
    if (!timelineColumn || timelineColumn->getElements().size() < 2) return;
    
    auto* timelineScrollable = static_cast<ScrollableColumn*>(timelineColumn->getElements()[0]);
    if (!timelineScrollable) return;
    
    // Get current tracks from engine
    const auto& engineTracks = app->getAllTracks();
    
    // Build set of expected track names (excluding Master)
    std::set<std::string> expectedTracks;
    for (const auto& track : engineTracks) {
        if (track->getName() != "Master") {
            expectedTracks.insert(track->getName());
        }
    }
    
    // Build set of existing track names in UI
    std::set<std::string> existingTracks;
    const auto& elements = timelineScrollable->getElements();
    for (auto* element : elements) {
        if (element && element->m_name.find("_track_row") != std::string::npos) {
            std::string trackName = element->m_name;
            // Remove "_track_row" suffix
            if (trackName.size() > 10) {
                trackName = trackName.substr(0, trackName.size() - 10);
                existingTracks.insert(trackName);
            }
        }
    }
    
    // Remove tracks that no longer exist in engine
    for (const auto& trackName : existingTracks) {
        if (expectedTracks.find(trackName) == expectedTracks.end()) {
            // Remove track from UI
            const auto& elements = timelineScrollable->getElements();
            for (int i = elements.size() - 1; i >= 0; i--) {
                if (elements[i] && elements[i]->m_name == trackName + "_track_row") {
                    timelineScrollable->removeElement(elements[i]);
                    // Also remove the spacer above it if it exists
                    if (i > 0 && elements[i-1] && elements[i-1]->m_name.empty()) {
                        timelineScrollable->removeElement(elements[i-1]);
                    }
                    break;
                }
            }
        }
    }
    
    // Add new tracks that exist in engine but not in UI
    for (const auto& trackName : expectedTracks) {
        if (existingTracks.find(trackName) == existingTracks.end()) {
            // Add new track to UI
            auto* trackRow = newTrack(trackName);
            timelineScrollable->addElement(spacer(Modifier().setfixedHeight(4.f)));
            timelineScrollable->addElement(trackRow);
        }
    }
    
    // Rebuild clips for all existing tracks to sync with engine data
    for (const auto& trackName : expectedTracks) {
        rebuildTrackClips(trackName);
    }
}

void Timeline::syncScrollOffsets() {
    if (!layout) return;
    
    // Get all scrollable rows in the timeline
    std::vector<ScrollableRow*> scrollableRows;
    
    // Find timeline scrollable column
    auto* baseColumn = static_cast<Column*>(layout);
    if (!baseColumn || baseColumn->getElements().empty()) return;
    
    auto* timelineColumn = static_cast<Column*>(baseColumn->getElements()[0]);
    if (!timelineColumn || timelineColumn->getElements().size() < 2) return;
    
    auto* timelineScrollable = static_cast<ScrollableColumn*>(timelineColumn->getElements()[0]);
    auto* masterTrackRow = static_cast<Row*>(timelineColumn->getElements()[1]);
    
    if (timelineScrollable) {
        // Collect all scrollable rows from tracks in the scrollable column
        const auto& elements = timelineScrollable->getElements();
        for (auto* element : elements) {
            if (element && element->m_name.find("_track_row") != std::string::npos) {
                auto* trackRow = static_cast<Row*>(element);
                if (trackRow && !trackRow->getElements().empty()) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(trackRow->getElements()[0]);
                    if (scrollableRow) {
                        scrollableRows.push_back(scrollableRow);
                    }
                }
            }
        }
    }
    
    // Add master track scrollable row
    if (masterTrackRow && !masterTrackRow->getElements().empty()) {
        auto* masterScrollableRow = static_cast<ScrollableRow*>(masterTrackRow->getElements()[0]);
        if (masterScrollableRow) {
            scrollableRows.push_back(masterScrollableRow);
        }
    }
    
    // Find if any row has a different offset (indicating a scroll happened)
    bool offsetChanged = false;
    float newOffset = currentScrollOffset;
    
    for (auto* row : scrollableRows) {
        float rowOffset = row->getOffset();
        if (std::abs(rowOffset - currentScrollOffset) > 0.1f) {
            newOffset = rowOffset;
            offsetChanged = true;
            break;
        }
    }
    
    // If offset changed, sync all rows to the new offset
    if (offsetChanged) {
        // Clamp the scroll offset to prevent scrolling past 0 seconds
        // Since offset is negative when scrolling right, don't allow values below 0
        currentScrollOffset = std::min(0.0f, newOffset);
        
        for (auto* row : scrollableRows) {
            row->setOffset(currentScrollOffset);
        }
        
        // Update measure line positions based on scroll offset
        updateMeasureLineOffsets();
    }
}

void Timeline::handleInput() {
    // TODO: Process user input
    handleClipDrag();
    handleClipResize();
    handleViewNavigation();
}

void Timeline::handleClipDrag() {
    // TODO: Handle clip dragging
}

void Timeline::handleClipResize() {
    // TODO: Handle clip resizing  
}

void Timeline::handleViewNavigation() {
    // TODO: Handle zoom and scroll
}

double Timeline::xPosToSeconds(float xPos) {
    // Convert screen X position to timeline seconds, accounting for scroll offset
    const float BASE_BEAT_WIDTH = 100.0f;
    float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
    double bpm = app->getBpm();
    double beatsPerSecond = bpm / 60.0;
    double pixelsPerSecond = beatWidth * beatsPerSecond;
    
    // Account for the current scroll offset
    float adjustedXPos = xPos - currentScrollOffset;
    return adjustedXPos / pixelsPerSecond;
}

float Timeline::secondsToXPos(double seconds) {
    // Convert timeline seconds to screen X position, accounting for scroll offset
    const float BASE_BEAT_WIDTH = 100.0f;
    float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
    double bpm = app->getBpm();
    double beatsPerSecond = bpm / 60.0;
    double pixelsPerSecond = beatWidth * beatsPerSecond;
    
    // Account for the current scroll offset
    return (seconds * pixelsPerSecond) + currentScrollOffset;
}

float Timeline::getPixelsPerSecond() {
    // TODO: Calculate pixels per second based on zoom
    return 100.0f * zoom; // Base scale * zoom factor
}

bool Timeline::isTimeVisible(double timeSeconds) {
    double viewEnd = offsetSeconds + (getViewWidth() / getPixelsPerSecond());
    return timeSeconds >= offsetSeconds && timeSeconds <= viewEnd;
}

float Timeline::getViewWidth() {
    // TODO: Get actual timeline view width
    return 1920.0f; // Placeholder
}

sf::VertexArray Timeline::generateWaveform(const std::string& trackName) {
    // TODO: Generate waveform for track
    sf::VertexArray waveform;
    return waveform;
}

std::vector<sf::RectangleShape> Timeline::generateClips(const std::string& trackName) {
    // TODO: Generate clip rectangles for track
    std::vector<sf::RectangleShape> clips;
    return clips;
}

std::vector<sf::CircleShape> Timeline::generateAutomationPoints(const std::string& trackName, const std::string& paramPath) {
    // TODO: Generate automation points
    std::vector<sf::CircleShape> points;
    return points;
}

sf::VertexArray Timeline::generateAutomationLine(const std::string& trackName, const std::string& paramPath) {
    // TODO: Generate automation line
    sf::VertexArray line;
    return line;
}

std::vector<sf::RectangleShape> Timeline::generateMeasureLines(float trackHeight, float scrollOffset) {
    std::vector<sf::RectangleShape> measureLines;
    
    // Get time signature from engine
    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
    
    // Constants for measure line generation
    const float BASE_BEAT_WIDTH = 100.0f;
    const float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
    
    const sf::Color& lineColor = app->resources.activeTheme->line_color;
    sf::Color transparentLineColor = lineColor;
    transparentLineColor.a = 100;
    
    // Generate lines for a reasonable number of measures
    const int numMeasures = 100;
    const int subdivisionsPerBeat = 4;
    
    for (int measure = 0; measure < numMeasures; measure++) {
        // Generate all beats for this measure
        for (int beat = 0; beat < timeSigNum; beat++) {
            float beatX = (measure * timeSigNum + beat) * beatWidth + scrollOffset;
            
            // Create main beat line
            sf::RectangleShape beatLine;
            beatLine.setPosition({beatX, 0});
            beatLine.setSize({1.0f, trackHeight});
            beatLine.setFillColor(lineColor);
            measureLines.push_back(beatLine);
            
            // Generate subdivisions within this beat
            for (int subBeat = 1; subBeat < subdivisionsPerBeat; subBeat++) {
                float subBeatX = beatX + (subBeat * beatWidth / subdivisionsPerBeat);
                
                sf::RectangleShape subBeatLine;
                subBeatLine.setPosition({subBeatX, 0});
                subBeatLine.setSize({1.0f, trackHeight});
                subBeatLine.setFillColor(transparentLineColor);
                measureLines.push_back(subBeatLine);
            }
        }
    }
    
    return measureLines;
}

Container* Timeline::buildUILayout() {
    // Create main timeline scrollable column
    ScrollableColumn* timelineScrollable = scrollableColumn(
        Modifier(),
        contains{}, "timeline"
    );

    timelineScrollable->setScrollSpeed(40.f);

    // Add all tracks except Master
    for (const auto& track : app->getAllTracks()) {
        if (track->getName() == "Master") continue;
        
        auto* trackRow = newTrack(track->getName());
        timelineScrollable->addElement(spacer(Modifier().setfixedHeight(4.f)));
        timelineScrollable->addElement(trackRow);
    }

    // Create master track
    auto* masterTrackRow = newTrack("Master");

    // Create the main layout with timeline and master track
    return column(
        Modifier().align(Align::RIGHT), 
        contains{
            column(
                Modifier().setColor(app->resources.activeTheme->middle_color)
                          .align(Align::RIGHT | Align::BOTTOM), 
                contains{
                    timelineScrollable,
                    masterTrackRow
                }, 
                "base_timeline_column"
            )
        }
    );
}

Row* Timeline::newTrack(const std::string& trackName) {
    // Create simple track label column
    auto* trackLabelColumn = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color).setfixedWidth(200).align(Align::LEFT | Align::TOP),
        contains{
            text(
                Modifier().align(Align::LEFT | Align::CENTER_Y).setColor(sf::Color::Transparent),
                trackName,
                "",
                trackName + "_text"
            )
        }, 
        trackName + "_label"
    );

    // Create scrollable row for timeline content
    auto* scrollableRowElement = scrollableRow(
        Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains{
            // Container for clips - this will hold clips and spacers
            row(
                Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
                contains{}, trackName + "_clips_container"
            )
        }, trackName + "_scrollable_row"
    );
    
    // Set scroll speed for the track row
    scrollableRowElement->setScrollSpeed(40.f);
    
    // Preserve current scroll offset when adding new tracks
    scrollableRowElement->setOffset(currentScrollOffset);

    // Generate and add measure lines as custom geometry (background)
    const float trackHeight = 96.0f; // Match the fixed height from the track row
    auto measureLines = generateMeasureLines(trackHeight, currentScrollOffset);
    
    // Convert to drawable pointers for custom geometry
    std::vector<std::shared_ptr<sf::Drawable>> measureGeometry;
    measureGeometry.reserve(measureLines.size());
    for (auto& line : measureLines) {
        measureGeometry.push_back(std::make_shared<sf::RectangleShape>(std::move(line)));
    }
    scrollableRowElement->setCustomGeometry(measureGeometry);

    // Handle track interactions
    scrollableRowElement->m_modifier.onLClick([this, trackName]() {
        if (!app->getWindow().hasFocus()) return;
        
        // Check for Ctrl+click to add clips
        bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || 
                          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        
        if (ctrlPressed) {
            // Get mouse position relative to the window and convert to timeline coordinates
            sf::Vector2f mousePos = app->ui->getMousePosition();
            processClipAtPosition(trackName, mousePos, false);
        } else {
            app->setSelectedTrack(trackName);
        }
    });

    scrollableRowElement->m_modifier.onRClick([this, trackName]() {
        if (!app->getWindow().hasFocus()) return;
        
        // Right-click for clip removal
        sf::Vector2f mousePos = app->ui->getMousePosition();
        processClipAtPosition(trackName, mousePos, true);
    });

    // Return complete track row
    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_row_color)
            .setfixedHeight(96)
            .align(Align::TOP | Align::LEFT)
            .onLClick([this, trackName](){
                if (!app->getWindow().hasFocus()) return;
                
                // Check for Ctrl+click to place clip
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)) {
                    // Get mouse position from UILO
                    sf::Vector2f mousePos = app->ui->getMousePosition();
                    processClipAtPosition(trackName, mousePos, false);
                } else {
                    app->setSelectedTrack(trackName);
                }
            })
            .onRClick([this, trackName](){
                if (!app->getWindow().hasFocus()) return;
                
                // Check for Ctrl+right-click to remove clip
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)) {
                    // Get mouse position from UILO
                    sf::Vector2f mousePos = app->ui->getMousePosition();
                    processClipAtPosition(trackName, mousePos, true);
                } else {
                    app->setSelectedTrack(trackName);
                }
            }),
        contains{
            scrollableRowElement,
            trackLabelColumn
        }, trackName + "_track_row");
}

ScrollableRow* Timeline::newAutomationLane(const std::pair<std::string, std::string>& parameterName, const std::string& parentTrack) {
    // TODO: Create automation lane scrollable row
    std::string laneId = parentTrack + "_" + parameterName.first + "_" + parameterName.second;
    auto* automationRow = scrollableRow(
        Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains{}, laneId + "_scrollable_row"
    );
    
    // Set scroll speed for the automation lane
    automationRow->setScrollSpeed(40.f);
    
    // Preserve current scroll offset when adding new automation lanes
    automationRow->setOffset(currentScrollOffset);
    
    return automationRow;
}

void Timeline::updateMeasureLineOffsets() {
    if (!layout) return;
    
    // Get all scrollable rows in the timeline (same logic as syncScrollOffsets)
    std::vector<ScrollableRow*> scrollableRows;
    
    // Find timeline scrollable column
    auto* baseColumn = static_cast<Column*>(layout);
    if (!baseColumn || baseColumn->getElements().empty()) return;
    
    auto* timelineColumn = static_cast<Column*>(baseColumn->getElements()[0]);
    if (!timelineColumn || timelineColumn->getElements().size() < 2) return;
    
    auto* timelineScrollable = static_cast<ScrollableColumn*>(timelineColumn->getElements()[0]);
    auto* masterTrackRow = static_cast<Row*>(timelineColumn->getElements()[1]);
    
    if (timelineScrollable) {
        // Collect all scrollable rows from tracks in the scrollable column
        const auto& elements = timelineScrollable->getElements();
        for (auto* element : elements) {
            if (element && element->m_name.find("_track_row") != std::string::npos) {
                auto* trackRow = static_cast<Row*>(element);
                if (trackRow && !trackRow->getElements().empty()) {
                    auto* scrollableRow = static_cast<ScrollableRow*>(trackRow->getElements()[0]);
                    if (scrollableRow) {
                        scrollableRows.push_back(scrollableRow);
                    }
                }
            }
        }
    }
    
    // Add master track scrollable row
    if (masterTrackRow && !masterTrackRow->getElements().empty()) {
        auto* masterScrollableRow = static_cast<ScrollableRow*>(masterTrackRow->getElements()[0]);
        if (masterScrollableRow) {
            scrollableRows.push_back(masterScrollableRow);
        }
    }
    
    // Update measure lines for all scrollable rows
    for (auto* scrollableRow : scrollableRows) {
        if (scrollableRow) {
            // Regenerate measure lines with the current scroll offset
            const float trackHeight = 96.0f;
            auto measureLines = generateMeasureLines(trackHeight, currentScrollOffset);
            
            // Convert to drawable pointers for custom geometry
            std::vector<std::shared_ptr<sf::Drawable>> measureGeometry;
            measureGeometry.reserve(measureLines.size());
            for (auto& line : measureLines) {
                measureGeometry.push_back(std::make_shared<sf::RectangleShape>(std::move(line)));
            }
            
            // Update the custom geometry on this scrollable row
            scrollableRow->setCustomGeometry(measureGeometry);
        }
    }
}

void Timeline::syncWithScrubber() {
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
    float currentTimelineOffset = currentScrollOffset;
    
    // Check if the current scroll offset differs from expected (indicates manual scroll)
    if (std::abs(currentScrollOffset - expectedTimelineOffset) > 0.01f) {
        timelineWasManuallyScrolled = true;
        currentTimelineOffset = currentScrollOffset;
    }
    
    // Apply scrubber-to-timeline sync if scrubber moved and timeline wasn't manually scrolled
    if (scrubberPositionChanged && !timelineWasManuallyScrolled) {
        double scrubberTimeSeconds = scrubberPos * lastClipEndSeconds;
        const float BASE_BEAT_WIDTH = 100.0f;
        float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
        float scrubberPixelPos = secondsToXPos(scrubberTimeSeconds);
        
        // Clamp the scroll offset to prevent scrolling past 0 seconds
        currentScrollOffset = std::min(0.0f, -scrubberPixelPos);
        expectedTimelineOffset = currentScrollOffset;
        
        // Update all scrollable rows to match scrubber position
        if (layout) {
            auto* baseColumn = static_cast<Column*>(layout);
            if (baseColumn && !baseColumn->getElements().empty()) {
                auto* timelineColumn = static_cast<Column*>(baseColumn->getElements()[0]);
                if (timelineColumn && timelineColumn->getElements().size() >= 2) {
                    auto* timelineScrollable = static_cast<ScrollableColumn*>(timelineColumn->getElements()[0]);
                    auto* masterTrackRow = static_cast<Row*>(timelineColumn->getElements()[1]);
                    
                    // Update track scrollable rows
                    if (timelineScrollable) {
                        for (auto* element : timelineScrollable->getElements()) {
                            if (element && element->m_name.find("_track_row") != std::string::npos) {
                                auto* trackRow = static_cast<Row*>(element);
                                if (trackRow && !trackRow->getElements().empty()) {
                                    auto* scrollableRow = static_cast<ScrollableRow*>(trackRow->getElements()[0]);
                                    if (scrollableRow) {
                                        scrollableRow->setOffset(currentScrollOffset);
                                    }
                                }
                            }
                        }
                    }
                    
                    // Update master track scrollable row
                    if (masterTrackRow && !masterTrackRow->getElements().empty()) {
                        auto* masterScrollableRow = static_cast<ScrollableRow*>(masterTrackRow->getElements()[0]);
                        if (masterScrollableRow) {
                            masterScrollableRow->setOffset(currentScrollOffset);
                        }
                    }
                }
            }
        }
        
        lastScrubberPosition = scrubberPos;
        updateMeasureLineOffsets();
    }
    
    // Update scrubber position if timeline was manually scrolled
    if (timelineWasManuallyScrolled) {
        double currentTimeSeconds = xPosToSeconds(-currentScrollOffset);
        currentTimeSeconds = std::max(0.0, std::min(currentTimeSeconds, lastClipEndSeconds));
        float newScrubberPos = lastClipEndSeconds > 0.0 ? static_cast<float>(currentTimeSeconds / lastClipEndSeconds) : 0.0f;
        
        app->writeConfig("scrubber_position", newScrubberPos);
        lastScrubberPosition = newScrubberPos;
        
        expectedTimelineOffset = currentScrollOffset;
    }
}

double Timeline::snapToGrid(double timeValue, bool forceSnap) const {
    bool shouldSnap = forceSnap || !isShiftPressed();
    if (!shouldSnap) return timeValue;
    
    const double bpm = app->getBpm();
    const double beatDuration = 60.0 / bpm;
    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
    
    // Snap to beat subdivisions (16th notes by default)
    const double snapResolution = beatDuration / 4.0; // 16th notes
    
    return std::round(timeValue / snapResolution) * snapResolution;
}

bool Timeline::isShiftPressed() const {
    return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || 
           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
}

float Timeline::getNearestMeasureX(const sf::Vector2f& mousePos) const {
    const float BASE_BEAT_WIDTH = 100.0f;
    const float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
    auto [timeSigNum, timeSigDen] = app->getTimeSignature();
    
    // Generate measure/beat positions
    const int subdivisionsPerBeat = 4; // 16th note subdivisions
    const float subBeatWidth = beatWidth / subdivisionsPerBeat;
    
    // Find the nearest subdivision line
    float adjustedMouseX = mousePos.x - currentScrollOffset;
    float nearestSnapX = std::round(adjustedMouseX / subBeatWidth) * subBeatWidth;
    
    return nearestSnapX + currentScrollOffset;
}

void Timeline::processClipAtPosition(const std::string& trackName, const sf::Vector2f& localMousePos, bool isRightClick) {
    Track* track = nullptr;
    
    // Find the track
    for (const auto& t : app->getAllTracks()) {
        if (t->getName() == trackName) {
            track = t.get(); // Get raw pointer from unique_ptr
            break;
        }
    }
    
    if (!track) return;
    
    float timePosition;
    
    if (isRightClick) {
        // Right-click: no snapping, just use direct position
        timePosition = xPosToSeconds(localMousePos.x);
    } else {
        // Left-click with Ctrl: snap to nearest measure/subdivision
        float snapX = getNearestMeasureX(localMousePos);
        timePosition = xPosToSeconds(snapX);
    }
    
    // Apply grid snapping
    timePosition = snapToGrid(timePosition, !isRightClick);
    
    // Prevent negative time positions
    timePosition = std::max(0.0f, timePosition);
    
    if (isRightClick) {
        // Remove clip at position
        if (track->getType() == Track::TrackType::MIDI) {
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            const auto& midiClips = midiTrack->getMIDIClips();
            for (size_t i = 0; i < midiClips.size(); ++i) {
                if (timePosition >= midiClips[i].startTime && 
                    timePosition <= (midiClips[i].startTime + midiClips[i].duration)) {
                    midiTrack->removeMIDIClip(i);
                    break;
                }
            }
        } else {
            const auto& clips = track->getClips();
            for (size_t i = 0; i < clips.size(); ++i) {
                if (timePosition >= clips[i].startTime && 
                    timePosition <= (clips[i].startTime + clips[i].duration)) {
                    track->removeClip(i);
                    break;
                }
            }
        }
        
        // Rebuild the visual representation of clips for this track
        rebuildTrackClips(trackName);
    } else {
        // Add clip at position
        if (track->getType() == Track::TrackType::MIDI) {
            double beatDuration = 60.0 / app->getBpm();
            MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
            
            // Check for collisions
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
                app->setSelectedTrack(track->getName());
            }
        } else {
            // Audio track
            if (track->getReferenceClip()) {
                AudioClip* refClip = track->getReferenceClip();
                
                // Check for collisions
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
                    app->setSelectedTrack(track->getName());
                }
            }
        }
        
        // Rebuild the visual representation of clips for this track
        rebuildTrackClips(trackName);
    }
}

float Timeline::timeToPixels(double timeSeconds) const {
    const float BASE_BEAT_WIDTH = 100.0f;
    float beatWidth = BASE_BEAT_WIDTH * zoom * app->ui->getScale();
    double bpm = app->getBpm();
    double beatsPerSecond = bpm / 60.0;
    double pixelsPerSecond = beatWidth * beatsPerSecond;
    
    return timeSeconds * pixelsPerSecond;
}

Container* Timeline::createClipContainer(double startTime, double duration, const std::string& clipId) {
    float clipWidth = timeToPixels(duration);
    
    auto* clipLabel = text(
        Modifier().align(Align::CENTER_X | Align::CENTER_Y).setColor(sf::Color::White),
        "Clip",
        "",
        clipId + "_text"
    );
    
    return row(
        Modifier()
            .setfixedWidth(clipWidth)
            .setHeight(50.f)
            .setColor(app->resources.activeTheme->clip_color),
        contains{
            clipLabel
        },
        clipId
    );
}

void Timeline::rebuildTrackClips(const std::string& trackName) {
    // Find the clips container for this track
    if (!layout) return;
    
    auto* baseColumn = static_cast<Column*>(layout);
    if (!baseColumn || baseColumn->getElements().empty()) return;
    
    auto* timelineColumn = static_cast<Column*>(baseColumn->getElements()[0]);
    if (!timelineColumn || timelineColumn->getElements().size() < 2) return;
    
    auto* timelineScrollable = static_cast<ScrollableColumn*>(timelineColumn->getElements()[0]);
    if (!timelineScrollable) return;
    
    // Find the track row
    Row* trackRow = nullptr;
    for (auto* element : timelineScrollable->getElements()) {
        if (element && element->m_name == trackName + "_track_row") {
            trackRow = static_cast<Row*>(element);
            break;
        }
    }
    
    if (!trackRow || trackRow->getElements().size() < 2) return;
    
    // Get the scrollable row (should be first element)
    auto* scrollableRow = static_cast<ScrollableRow*>(trackRow->getElements()[0]);
    if (!scrollableRow || scrollableRow->getElements().empty()) return;
    
    // Get the clips container (should be first element in scrollable row)
    auto* clipsContainer = static_cast<Row*>(scrollableRow->getElements()[0]);
    if (!clipsContainer) return;
    
    // Clear existing clips
    while (!clipsContainer->getElements().empty()) {
        clipsContainer->removeElement(clipsContainer->getElements()[0]);
    }
    
    // Find the track to get clip data
    Track* track = nullptr;
    for (const auto& t : app->getAllTracks()) {
        if (t->getName() == trackName) {
            track = t.get();
            break;
        }
    }
    
    if (!track) return;
    
    // Collect all clips with their positions
    struct ClipInfo {
        double startTime;
        double duration;
        std::string id;
    };
    
    std::vector<ClipInfo> clipInfos;
    
    if (track->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(track);
        const auto& midiClips = midiTrack->getMIDIClips();
        for (size_t i = 0; i < midiClips.size(); ++i) {
            const auto& clip = midiClips[i];
            clipInfos.push_back({clip.startTime, clip.duration, trackName + "_midi_clip_" + std::to_string(i)});
        }
    } else {
        const auto& audioClips = track->getClips();
        for (size_t i = 0; i < audioClips.size(); ++i) {
            const auto& clip = audioClips[i];
            clipInfos.push_back({clip.startTime, clip.duration, trackName + "_audio_clip_" + std::to_string(i)});
        }
    }
    
    // Sort clips by start time
    std::sort(clipInfos.begin(), clipInfos.end(), [](const ClipInfo& a, const ClipInfo& b) {
        return a.startTime < b.startTime;
    });
    
    // Build layout with spacers and clips
    double currentTime = 0.0;
    
    for (const auto& clipInfo : clipInfos) {
        // Add spacer before clip if needed
        if (clipInfo.startTime > currentTime) {
            double spacerDuration = clipInfo.startTime - currentTime;
            float spacerWidth = timeToPixels(spacerDuration);
            
            clipsContainer->addElement(spacer(Modifier().setfixedWidth(spacerWidth)));
        }
        
        // Add the clip container
        auto* clipContainer = createClipContainer(clipInfo.startTime, clipInfo.duration, clipInfo.id);
        clipsContainer->addElement(clipContainer);
        
        currentTime = clipInfo.startTime + clipInfo.duration;
    }
}

GET_INTERFACE
DECLARE_PLUGIN(Timeline)