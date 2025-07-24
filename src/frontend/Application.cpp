#include "Application.hpp"

Application::Application() {
    // Initialize window and engine
    screenResolution = sf::VideoMode::getDesktopMode();

    screenResolution.size.x = screenResolution.size.x / 1.5f;
    screenResolution.size.y = screenResolution.size.y / 1.5f;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    // settings.sRgbCapable = true;

    windowView.setSize({
        (float)screenResolution.size.x,
        (float)screenResolution.size.y
    });

    windowView.setCenter({
        (float)screenResolution.size.x / 2.f,
        (float)screenResolution.size.y / 2.f
    });

    window.create(screenResolution, "MULO", sf::Style::Default, sf::State::Windowed, settings);
    window.setVerticalSyncEnabled(true);

    engine.newComposition("untitled");
    engine.addTrack("Master");
    initUIResources();

    // UI Elements
    topRowElement               = topRow();
    fileBrowserElement          = fileBrowser();
    masterTrackElement          = masterTrack();
    timelineElement             = timeline();
    mixerElement                = mixer();
    masterMixerTrackElement     = masterMixerTrack();
    browserAndTimelineElement   = browserAndTimeline();
    browserAndMixerElement      = browserAndMixer();
    fxRackElement               = fxRack();
    timelineElement->setScrollSpeed(20.f);
    mixerElement->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(sf::Color(50, 50, 50)),
        contains {
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onLClick([&](){std::cout << "options" << std::endl;}), 
                ButtonStyle::Rect, "Options", resources.openSansFont, sf::Color::Black, "cm_options"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onLClick([&](){std::cout << "rename" << std::endl;}), 
                ButtonStyle::Rect, "Rename", resources.openSansFont, sf::Color::Black, "cm_rename"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onLClick([&](){std::cout << "change color" << std::endl;}), 
                ButtonStyle::Rect, "Change Color", resources.openSansFont, sf::Color::Black, "cm_change_color"),
            spacer(Modifier().setfixedHeight(1)),
        }
    );
    contextMenu->hide();

    // Base UI
    ui = new UILO(window, windowView, {{
        page({
            column(
                Modifier(),
            contains{
                topRowElement,
                browserAndTimelineElement,
                fxRackElement,
            }),
            contextMenu
        }), "timeline" }
    });

    // Mixer
    ui->addPage({
        page({
            column(
                Modifier(),
            contains{
                topRowElement,
                browserAndMixerElement,
                fxRackElement,
            }),
            contextMenu
        }), "mixer" }
    );

    running = ui->isRunning();

    ui->switchToPage("timeline");
    loadComposition("assets/empty_project.mpf");
    ui->forceUpdate();
}

Application::~Application() {
    delete ui;
    ui = nullptr;
}

void Application::update() {
    running = ui->isRunning();

    if (ui->isRunning() && running) {
        bool shouldForceUpdate = false;
        
        // Handle user interactions
        shouldForceUpdate |= handleContextMenu();
        shouldForceUpdate |= handleUIButtons();
        shouldForceUpdate |= handlePlaybackControls();
        shouldForceUpdate |= handleTrackEvents();
        shouldForceUpdate |= handleKeyboardShortcuts();
        shouldForceUpdate |= playing;

        // Update UI
        if (shouldForceUpdate) ui->forceUpdate();
        else ui->update(windowView);

        // Update timeline rendering
        updateTimelineRendering();
    }
}

void Application::updateTimelineRendering() {
    if (engine.getAllTracks().empty()) return;

    // Sync timeline scroll offset
    float newMasterOffset = timelineOffset;
    for (const auto& track : engine.getAllTracks()) {
        auto* scrollableRow = static_cast<ScrollableRow*>(containers[track->getName() + "_scrollable_row"]);
        scrollableRow->setScrollSpeed(20.f);
        if (scrollableRow->getOffset() != timelineOffset) {
            newMasterOffset = scrollableRow->getOffset();
            break;
        }
    }

    // Handle playhead following during playback
    if (playing) {
        newMasterOffset = calculatePlayheadFollowOffset(newMasterOffset);
    }

    float clampedOffset = std::min(0.f, newMasterOffset);
    
    // Render all timeline elements
    renderTimelineElements(clampedOffset);
    
    timelineOffset = clampedOffset;
}

float Application::calculatePlayheadFollowOffset(float currentOffset) {
    float playheadXPos = secondsToXPosition(engine.getBpm(), 100.f * uiState.timelineZoomLevel, engine.getPosition());
    float visibleWidth = 0.f;
    
    // Get visible width from any track
    for (const auto& track : engine.getAllTracks()) {
        auto* trackRow = containers[track->getName() + "_scrollable_row"];
        if (trackRow) {
            visibleWidth = trackRow->getSize().x;
            break;
        }
    }
    
    if (visibleWidth > 0.f) {
        float centerPos = visibleWidth * 0.5f;
        float targetOffset = -(playheadXPos - centerPos);
        float followSpeed = 0.08f;
        return currentOffset + (targetOffset - currentOffset) * followSpeed;
    }
    
    return currentOffset;
}

void Application::renderTimelineElements(float clampedOffset) {
    std::vector<std::shared_ptr<sf::Drawable>> allTimelineElements;

    // Process each track
    for (const auto& track : engine.getAllTracks()) {
        auto* trackRow = containers[track->getName() + "_scrollable_row"];
        auto* scrollableRow = static_cast<ScrollableRow*>(trackRow);

        scrollableRow->setScrollSpeed(20.f);
        scrollableRow->setOffset(clampedOffset);

        // Set up track interaction handlers
        setupTrackInteractionHandlers(track, trackRow, clampedOffset);

        // Generate and clip timeline elements for this track
        auto [clips, lines] = generateAndClipTrackElements(track, trackRow, scrollableRow, clampedOffset);
        
        // Add to collection
        allTimelineElements.insert(allTimelineElements.end(), clips.begin(), clips.end());
        allTimelineElements.insert(allTimelineElements.end(), lines.begin(), lines.end());

        // Clear individual track geometry
        std::vector<std::shared_ptr<sf::Drawable>> emptyGeometry;
        static_cast<uilo::Element*>(trackRow)->setCustomGeometry(emptyGeometry);
    }

    // Add playhead
    addPlayheadToTimeline(allTimelineElements, clampedOffset);
    
    // Set all elements to timeline for rendering
    timelineElement->setCustomGeometry(allTimelineElements);
}

void Application::setupTrackInteractionHandlers(Track* track, uilo::Element* trackRow, float clampedOffset) {
    // Left click - add clip
    trackRow->m_modifier.onLClick([&, track, clampedOffset](){
        sf::Vector2f globalMousePos = ui->getMousePosition();
        sf::Vector2f trackRowPos = trackRow->getPosition();
        sf::Vector2f localMousePos = globalMousePos - trackRowPos;
        
        auto* scrollableRow = static_cast<ScrollableRow*>(trackRow);
        sf::Vector2f scrollableSize = scrollableRow->getSize();
        
        auto lines = generateTimelineMeasures(0.f * uiState.timelineZoomLevel, clampedOffset, scrollableSize);
        float snapX = getNearestMeasureX(localMousePos, lines);
        float timePosition = xPosToSeconds(engine.getBpm(), 100.f * uiState.timelineZoomLevel, snapX - clampedOffset, clampedOffset);
        
        if (track->getReferenceClip()) {
            AudioClip* newClip = track->getReferenceClip();
            track->addClip(AudioClip(newClip->sourceFile, timePosition, 0.0, newClip->duration, 1.0f));
        }
        
        std::cout << "Added clip to track '" << track->getName() << "' at time: " << timePosition << " seconds" << std::endl;
    });

    // Right click - remove clip
    trackRow->m_modifier.onRClick([&, track, clampedOffset](){
        sf::Vector2f globalMousePos = ui->getMousePosition();
        sf::Vector2f trackRowPos = trackRow->getPosition();
        sf::Vector2f localMousePos = globalMousePos - trackRowPos;
        
        float timePosition = xPosToSeconds(engine.getBpm(), 100.f * uiState.timelineZoomLevel, localMousePos.x - clampedOffset, clampedOffset);
        
        auto clips = track->getClips();
        for (size_t i = 0; i < clips.size(); ++i) {
            if (timePosition >= clips[i].startTime && timePosition <= (clips[i].startTime + clips[i].duration)) {
                std::cout << "Removed clip from track '" << track->getName() << "' at time: " << clips[i].startTime << " seconds" << std::endl;
                track->removeClip(i);
                contextMenu->hide();
                break;
            }
        }
    });
}

std::pair<std::vector<std::shared_ptr<sf::Drawable>>, std::vector<std::shared_ptr<sf::Drawable>>> 
Application::generateAndClipTrackElements(Track* track, uilo::Element* trackRow, ScrollableRow* scrollableRow, float clampedOffset) {
    sf::Vector2f scrollableSize = scrollableRow->getSize();
    
    // Generate elements
    auto lines = generateTimelineMeasures(100.f * uiState.timelineZoomLevel, clampedOffset, scrollableSize);
    auto clips = generateClipRects(engine.getBpm(), 100.f * uiState.timelineZoomLevel, clampedOffset, scrollableSize, track->getClips());

    // Calculate positioning and clipping bounds
    float trackYOffset = trackRow->getPosition().y - timelineElement->getPosition().y;
    sf::Vector2f timelinePos = timelineElement->getPosition();
    sf::Vector2f trackRowPos = trackRow->getPosition();
    float scrollableAreaLeft = trackRowPos.x - timelinePos.x;
    float scrollableAreaRight = scrollableAreaLeft + scrollableSize.x;
    
    // Apply positioning and clipping to clips
    for (auto& clip : clips) {
        if (auto rect = std::dynamic_pointer_cast<sf::RectangleShape>(clip)) {
            clipAndPositionElement(rect, trackYOffset, scrollableAreaLeft, scrollableAreaRight);
        }
    }
    
    // Apply positioning and clipping to lines
    for (auto& line : lines) {
        if (auto rect = std::dynamic_pointer_cast<sf::RectangleShape>(line)) {
            clipAndPositionElement(rect, trackYOffset, scrollableAreaLeft, scrollableAreaRight);
        }
    }

    return {clips, lines};
}

void Application::clipAndPositionElement(std::shared_ptr<sf::RectangleShape> rect, float yOffset, float leftBound, float rightBound) {
    sf::Vector2f pos = rect->getPosition();
    sf::Vector2f size = rect->getSize();
    
    // Apply Y offset
    pos.y += yOffset;
    
    // Clip horizontally
    float elementRight = pos.x + size.x;
    if (elementRight > rightBound) {
        size.x = std::max(0.f, rightBound - pos.x);
    }
    
    if (pos.x < leftBound) {
        float clipAmount = leftBound - pos.x;
        pos.x = leftBound;
        size.x = std::max(0.f, size.x - clipAmount);
    }
    
    rect->setPosition(pos);
    rect->setSize(size);
}

void Application::addPlayheadToTimeline(std::vector<std::shared_ptr<sf::Drawable>>& allTimelineElements, float clampedOffset) {
    if (engine.getAllTracks().empty()) return;

    float timelineHeight = (engine.getAllTracks().size() - 1) * 98.f + 96.f;
    auto playheadDrawable = getPlayHead(engine.getBpm(), 100.f * uiState.timelineZoomLevel, clampedOffset, engine.getPosition(), sf::Vector2f(4.f, timelineHeight));
    
    if (auto playheadRect = std::dynamic_pointer_cast<sf::RectangleShape>(playheadDrawable)) {
        sf::Vector2f currentPos = playheadRect->getPosition();
        sf::Vector2f playheadSize = playheadRect->getSize();
        
        // Find first track Y position
        float firstTrackY = 2.f;
        const auto& timelineElements = timelineElement->getElements();
        if (!timelineElements.empty()) {
            for (const auto* element : timelineElements) {
                if (element && element->getType() == uilo::EType::Row) {
                    firstTrackY = element->getPosition().y - timelineElement->getPosition().y;
                    break;
                }
            }
        }
        
        // Clip playhead to scrollable area
        if (!engine.getAllTracks().empty()) {
            auto* firstTrackRow = containers[engine.getAllTracks()[0]->getName() + "_scrollable_row"];
            if (firstTrackRow) {
                auto* firstScrollableRow = static_cast<ScrollableRow*>(firstTrackRow);
                sf::Vector2f timelinePos = timelineElement->getPosition();
                sf::Vector2f firstTrackPos = firstTrackRow->getPosition();
                sf::Vector2f firstScrollableSize = firstScrollableRow->getSize();
                
                float scrollableAreaLeft = firstTrackPos.x - timelinePos.x;
                float scrollableAreaRight = scrollableAreaLeft + firstScrollableSize.x;
                
                float playheadRight = currentPos.x + playheadSize.x;
                if (playheadRight > scrollableAreaRight) {
                    playheadSize.x = std::max(0.f, scrollableAreaRight - currentPos.x);
                    playheadRect->setSize(playheadSize);
                }
            }
        }
        
        playheadRect->setPosition({currentPos.x, firstTrackY});
    }
    
    allTimelineElements.push_back(playheadDrawable);
}

bool Application::handleContextMenu() {
    static bool prevRightClick = false;
    static bool contextMenuJustShown = false;
    bool rightClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool shouldForceUpdate = false;

    if (rightClick && !prevRightClick) {
        contextMenu->setPosition(ui->getMousePosition());
        contextMenu->show();
        contextMenuJustShown = true;
        shouldForceUpdate = true;
    }

    if (contextMenu->m_modifier.isVisible() && !contextMenuJustShown) {
        const auto intersection = contextMenu->getBounds().findIntersection(sf::FloatRect(ui->getMousePosition(), {20, 20}));
        if (!intersection) {
            std::cout << "hide" << std::endl;
            contextMenu->hide();
            shouldForceUpdate = true;
        }
    }
    
    if (contextMenuJustShown && !rightClick) {
        contextMenuJustShown = false;
    }
    
    prevRightClick = rightClick;
    return shouldForceUpdate;
}

bool Application::handleUIButtons() {
    bool shouldForceUpdate = false;

    if (buttons["select_directory"]->isClicked()) {
        std::string dir = selectDirectory();
        if (!dir.empty()) {
            uiState.fileBrowserDirectory = dir;
            std::cout << "Selected directory: " << dir << std::endl;
        }
        shouldForceUpdate = true;
    }

    if (buttons["mixer"]->isClicked()) {
        showMixer = !showMixer;

        if (showMixer)
            ui->switchToPage("mixer");
        else
            ui->switchToPage("timeline");

        shouldForceUpdate = true;
    }

    if (buttons["new_track"]->isClicked()) {
        uiChanged = true;
        newTrack();
        std::cout << "New track added. Total tracks: " << uiState.trackCount << std::endl;
        shouldForceUpdate = true;
    }

    if (buttons["save"]->isClicked()) {
        if (engine.saveState(selectDirectory() + "/" + engine.getCurrentCompositionName() + ".mpf"))
            std::cout << "Project saved successfully." << std::endl;
        else
            std::cerr << "Failed to save project." << std::endl;
        shouldForceUpdate = true;
    }

    if (buttons["load"]->isClicked()) {
        std::string path = selectFile({"*.mpf"});
        if (!path.empty()) {
            loadComposition(path);
        }
        else
            std::cout << "No file selected." << std::endl;
        shouldForceUpdate = true;
    }

    return shouldForceUpdate;
}

bool Application::handlePlaybackControls() {
    static bool prevSpace = false;
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool shouldForceUpdate = false;

    if ((buttons["play"]->isClicked() || (space && !prevSpace)) && !playing) {
        std::cout << "Playing audio..." << std::endl;
        engine.play();
        buttons["play"]->setText("pause");
        playing = true;
        shouldForceUpdate = true;
    }
    else if ((buttons["play"]->isClicked() || (space && !prevSpace)) && playing) {
        std::cout << "Pausing audio..." << std::endl;
        engine.pause();
        engine.setPosition(0.0);
        buttons["play"]->setText("play");
        playing = false;
        shouldForceUpdate = true;
    }

    prevSpace = space;
    return shouldForceUpdate;
}

bool Application::handleKeyboardShortcuts() {
    static bool prevCtrl = false, prevZ = false, prevY = false;
    
    bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
    bool z = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z);
    bool y = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Y);
    bool shouldForceUpdate = false;

    // Block UILO input when Ctrl is pressed to capture scroll events
    if (ctrl != prevCtrl) {
        ui->setInputBlocked(ctrl);
    }

    if (ctrl && !prevCtrl) prevZ = prevY = false;

    // Handle Ctrl+Z (Undo)
    if (ctrl && z && !prevZ) {
        undo();
        std::cout << "Undo, undoStack size: " << undoStack.size() << std::endl;
        shouldForceUpdate = true;
    }
    
    // Handle Ctrl+Y (Redo)
    if (ctrl && y && !prevY) {
        redo();
        std::cout << "Redo, redoStack size: " << redoStack.size() << std::endl;
        shouldForceUpdate = true;
    }

    // Handle Ctrl+Scroll for timeline zoom
    if (ctrl) {
        float scrollDelta = ui->getVerticalScrollDelta();
        
        if (scrollDelta != 0.0f) {
            const float zoomSpeed = 0.1f;
            const float minZoom = 0.1f;
            const float maxZoom = 5.0f;
            
            if (scrollDelta > 0) {
                uiState.timelineZoomLevel = std::max(minZoom, uiState.timelineZoomLevel - zoomSpeed);
            } else {
                uiState.timelineZoomLevel = std::min(maxZoom, uiState.timelineZoomLevel + zoomSpeed);
            }
            
            ui->resetScrollDeltas();
            shouldForceUpdate = true;
        }
    }

    prevCtrl = ctrl;
    prevZ = z;
    prevY = y;
    return shouldForceUpdate;
}

void Application::render() {
    if (ui->windowShouldUpdate()) {
        window.clear(sf::Color::Black);

        // Draw Stuff Here

        ui->render();

        // Or Here

        window.display();
    }
}

bool Application::isRunning() const {
    return running;
}

void Application::initUIResources() {
    juce::File fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/DejaVuSans.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/DejaVuSans.ttf");
    }
    resources.openSansFont = fontFile.getFullPathName().toStdString();
}

Row* Application::topRow() {
    return row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(64)
            .setColor(sf::Color(200, 200, 200)),
    contains{
        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill, 
            "load", 
            resources.openSansFont, 
            sf::Color(230, 230, 230),
            "load"
        ),

        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "save",
            resources.openSansFont,
            sf::Color::White,
            "save"
        ),

        button(
            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "play",
            resources.openSansFont,
            sf::Color::White,
            "play"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "mixer",
            resources.openSansFont,
            sf::Color::White,
            "mixer"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "+ track",
            resources.openSansFont,
            sf::Color::White,
            "new_track"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),
    });
}

Row* Application::browserAndTimeline() {
    return row(
        Modifier().setWidth(1.f).setHeight(1.f),
    contains{
        fileBrowserElement,
        row(
            Modifier()
                .setWidth(1.f)
                .setHeight(1.f)
                .setColor(sf::Color(100, 100, 100)),
        contains{
            column(
                Modifier()
                    .setWidth(1.f)
                    .setHeight(1.f)
                    .align(Align::LEFT | Align::TOP),
            contains{
                timelineElement,
                masterTrackElement,
            }),
        })
    });
}

Row* Application::browserAndMixer() {
    return row(
        Modifier().setWidth(1.f).setHeight(1.f),
    contains{
        fileBrowserElement,
        masterMixerTrackElement,
        mixerElement
    });
}

Column* Application::fileBrowser() {
    return column(
        Modifier()
            .align(Align::LEFT)
            .setfixedWidth(256)
            .setColor(sf::Color(155, 155, 155)),
    contains{
        spacer(Modifier().setfixedHeight(16).align(Align::TOP)),

        button(
            Modifier()
                .setfixedHeight(48)
                .setWidth(0.8f)
                .setColor(sf::Color(120, 120, 120))
                .align(Align::CENTER_X),
            ButtonStyle::Pill,
            "Select Directory",
            resources.openSansFont,
            sf::Color::White,
            "select_directory"
        ),
    });
}

ScrollableColumn* Application::timeline() {
    return scrollableColumn(
        Modifier(),
    contains{
    }, "timeline" );
}

Row* Application::fxRack() {
    return row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(256)
            .setColor(sf::Color(200, 200, 200))
            .align(Align::BOTTOM),
    contains{
        
    });
}

Row* Application::track(const std::string& trackName, Align alignment, float volume, float pan) {
    std::cout << "Creating track: " << trackName << std::endl;
    return row(
        Modifier()
            .setColor(sf::Color(120, 120, 120))
            .setfixedHeight(96)
            .align(alignment),
    contains{
        scrollableRow(
            Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains {
            // contains nothing, really just to get the offset from scroll
        }, trackName + "_scrollable_row"),

        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(track_color),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            row(
                Modifier().align(Align::RIGHT),
            contains{
                spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

                column(
                    Modifier(),
                contains{
                    text(
                        Modifier().setColor(sf::Color(25, 25, 25)).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        trackName,
                        resources.openSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(sf::Color(50, 50, 50)),
                            ButtonStyle::Rect,
                            "mute",
                            resources.openSansFont,
                            sf::Color::White,
                            "mute_" + trackName
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    sf::Color::White,
                    sf::Color::Black,
                    trackName + "_volume_slider"
                ),

                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            }),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        })
    }, trackName + "_track_row");
}

Column* Application::mixerTrack(const std::string& trackName, Align alignment, float volume, float pan) {
    return column(
        Modifier()
            .setColor(track_color)
            .setfixedWidth(96)
            .align(alignment),
    contains{
        spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
        text(
            Modifier().setColor(sf::Color(25, 25, 25)).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
            trackName,
            resources.openSansFont
        ),

        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        slider(
            Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
            sf::Color::White,
            sf::Color::Black,
            trackName + "_mixer_volume_slider"
        ),
        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        button(
            Modifier()
                .setfixedHeight(32)
                .setfixedWidth(64)
                .align(Align::CENTER_X | Align::BOTTOM)
                .setColor(sf::Color::Red),
            ButtonStyle::Rect,
            "solo",
            resources.openSansFont,
            sf::Color::White,
            "solo_" + trackName
        ),
    });
}

Column* Application::masterMixerTrack(const std::string& trackName, Align alignment, float volume, float pan) {
    return column(
        Modifier()
            .setColor(master_track_color)
            .setfixedWidth(96)
            .align(alignment),
    contains{
        spacer(Modifier().setfixedHeight(12).align(Align::TOP | Align::CENTER_X)),
        text(
            Modifier().setColor(sf::Color(25, 25, 25)).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
            trackName,
            resources.openSansFont
        ),

        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        slider(
            Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
            sf::Color::White,
            sf::Color::Black,
            "Master_mixer_volume_slider"
        ),
        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        button(
            Modifier()
                .setfixedHeight(32)
                .setfixedWidth(64)
                .align(Align::CENTER_X | Align::BOTTOM)
                .setColor(sf::Color::Red),
            ButtonStyle::Rect,
            "solo",
            resources.openSansFont,
            sf::Color::White,
            "solo_Master"
        ),
    });
}

Row* Application::masterTrack() {
    return row(
        Modifier()
            .setColor(sf::Color(120, 120, 120))
            .setfixedHeight(96)
            .align(Align::LEFT | Align::BOTTOM),
    contains{
        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(sf::Color(155, 155, 155)),
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
                        Modifier().setColor(sf::Color(25, 25, 25)).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        "Master",
                        resources.openSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(sf::Color(50, 50, 50)),
                            ButtonStyle::Rect,
                            "mute",
                            resources.openSansFont,
                            sf::Color::White,
                            "mute_Master"
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    sf::Color::White,
                    sf::Color::Black,
                    "Master_volume_slider"
                ),

                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            }, "Master_Track_Label"),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        }, "Master_Track_Column")
    }, "Master_Track");
}

ScrollableRow* Application::mixer() {
    return scrollableRow(
        Modifier().setWidth(1.f).setHeight(1.f).setColor(sf::Color(100, 100, 100)),
    contains{
       
    }, "mixer");
}

std::string Application::selectDirectory() {
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cerr << "FileChooser must be called from the message thread" << std::endl;
        return "";
    }
    
    juce::FileChooser chooser("Select directory", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");
    
    if (chooser.browseForDirectory()) {
        juce::File result = chooser.getResult();
        if (result.exists()) {
            return result.getFullPathName().toStdString();
        }
    }
    
    return "";
}

std::string Application::selectFile(std::initializer_list<std::string> filters) {
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cerr << "FileChooser must be called from the message thread" << std::endl;
        return "";
    }
    
    juce::String filterString;
    for (auto it = filters.begin(); it != filters.end(); ++it) {
        if (it != filters.begin())
            filterString += ";";
        filterString += juce::String((*it).c_str());
    }

    std::cout << "Filter string: " << filterString << std::endl;

    juce::FileChooser chooser("Select audio file", juce::File(), filterString);
    if (chooser.browseForFileToOpen()) {
        return chooser.getResult().getFullPathName().toStdString();
    }
    return "";
}

void Application::newTrack() {
    std::string samplePath = selectFile({"*.wav", "*.mp3", "*.flac"});
    if (samplePath.empty()) return;

    std::string trackName;
    int trackIndex = uiState.trackCount;

    if (!samplePath.empty()) {
        juce::File sampleFile(samplePath);
        trackName = sampleFile.getFileNameWithoutExtension().toStdString();
        engine.addTrack(trackName);

        // Use AudioFormatReader for accurate length in seconds
        double lengthSeconds = 0.0;
        if (auto* reader = engine.formatManager.createReaderFor(sampleFile)) {
            lengthSeconds = reader->lengthInSamples / reader->sampleRate;
            delete reader;
        }

        engine.getTrack(trackIndex)->setReferenceClip(AudioClip(sampleFile, 0.0, 0.0, lengthSeconds, 1.0f));
        std::cout << "Loaded sample: " << samplePath << " into Track '" << trackName << "' (" << (trackIndex + 1) << ")" << std::endl;
    } 
    else {
        trackName = "Track_" + std::to_string(trackIndex + 1);
        engine.addTrack(trackName);
        std::cout << "No sample selected for Track '" << trackName << "' (" << (trackIndex + 1) << ")" << std::endl;
    }

    uiState.trackCount++;

    timelineElement->addElements({
        spacer(Modifier().setfixedHeight(2).align(Align::TOP)),
        track(trackName, Align::TOP | Align::LEFT),
    });

    mixerElement->addElements({
        spacer(Modifier().setfixedWidth(2).align(Align::LEFT)),
        mixerTrack(trackName, Align::TOP | Align::LEFT),
    });
}

void Application::loadComposition(const std::string& path) {
    uiState = UIState(); // Reset UI state
    timelineElement->clear();
    mixerElement->clear();
    engine.loadComposition(path);

    // timelineElement->addElement(
    //     masterTrackElement
    // );

    // mixerElement->addElement(
    //     masterMixerTrackElement
    // );

    uiState.trackCount = engine.getAllTracks().size();

    for (auto& track : engine.getAllTracks())
        if (!track->getReferenceClip() && !track->getClips().empty())
            track->setReferenceClip(track->getClips()[0]);
    
    for (auto& t : engine.getAllTracks()) {
        std::cout << "Loaded track: " << t->getName() << std::endl;

        if (t->getName() == "Master") {
            uiState.masterTrack.pan = t->getPan();
            uiState.masterTrack.volume = t->getVolume();
            uiState.masterTrack.name = t->getName();

            continue;
        }

        else {
            uiState.tracks[t->getName()].clips = t->getClips();
            uiState.tracks[t->getName()].name = t->getName();
            uiState.tracks[t->getName()].volume = t->getVolume();
            uiState.tracks[t->getName()].pan = t->getPan();

            timelineElement->addElements({
                spacer(Modifier().setfixedHeight(2).align(Align::TOP)),
                track(t->getName(), Align::TOP | Align::LEFT),
            });

            mixerElement->addElements({
                spacer(Modifier().setfixedWidth(2).align(Align::LEFT)),
                mixerTrack(t->getName(), Align::TOP | Align::LEFT, decibelsToFloat(t->getVolume()), t->getPan()),
            });

            getSlider(t->getName() + "_volume_slider")->setValue(decibelsToFloat(t->getVolume()));
            getSlider(t->getName() + "_mixer_volume_slider")->setValue(decibelsToFloat(t->getVolume()));
        }
    }

    undoStack.push(engine.getStateString());
    // redoStack = std::stack<std::string>();
}

bool Application::handleTrackEvents() {
    bool shouldForceUpdate = false;

    if (getButton("mute_Master")->isClicked()) {
        engine.getMasterTrack()->toggleMute();
        getButton("mute_Master")->m_modifier.setColor((engine.getMasterTrack()->isMuted() ? mute_color : sf::Color(50, 50, 50)));
        std::cout << "Master track mute state toggled to " << ((engine.getMasterTrack()->isMuted()) ? "true" : "false") << std::endl;

        shouldForceUpdate = true;
    }

    if (getSlider("Master_volume_slider")->getValue() != decibelsToFloat(engine.getMasterTrack()->getVolume())) {
        float newVolume = floatToDecibels(getSlider("Master_volume_slider")->getValue());
        engine.getMasterTrack()->setVolume(newVolume);
        getSlider("Master_mixer_volume_slider")->setValue(getSlider("Master_volume_slider")->getValue());
        std::cout << "Master track volume changed to: " << newVolume << " db" << std::endl;

        shouldForceUpdate = true;
    }

    if (getSlider("Master_mixer_volume_slider")->getValue() != decibelsToFloat(engine.getMasterTrack()->getVolume())) {
        float newVolume = floatToDecibels(getSlider("Master_mixer_volume_slider")->getValue());
        engine.getMasterTrack()->setVolume(newVolume);
        getSlider("Master_volume_slider")->setValue(getSlider("Master_mixer_volume_slider")->getValue());
        std::cout << "Master track volume changed to: " << newVolume << " db" << std::endl;

        shouldForceUpdate = true;
    }

    for (auto& track : engine.getAllTracks()) {
        if (getButton("mute_" + track->getName())->isClicked()) {
            track->toggleMute();
            getButton("mute_" + track->getName())->m_modifier.setColor((track->isMuted() ? mute_color : sf::Color(50, 50, 50)));
            std::cout << "Track '" << track->getName() << "' mute state toggled to " << ((track->isMuted()) ? "true" : "false") << std::endl;

            shouldForceUpdate = true;
        }

        if (floatToDecibels(getSlider(track->getName() + "_volume_slider")->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(getSlider(track->getName() + "_volume_slider")->getValue());
            track->setVolume(newVolume);
            getSlider(track->getName() + "_mixer_volume_slider")->setValue(getSlider(track->getName() + "_volume_slider")->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;

            shouldForceUpdate = true;
        }

        if (floatToDecibels(getSlider(track->getName() + "_mixer_volume_slider")->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(getSlider(track->getName() + "_mixer_volume_slider")->getValue());
            track->setVolume(newVolume);
            getSlider(track->getName() + "_volume_slider")->setValue(getSlider(track->getName() + "_mixer_volume_slider")->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;

            shouldForceUpdate = true;
        }
    }
    return shouldForceUpdate;
}

void Application::rebuildUIFromEngine() {
    uiState = UIState();
    timelineElement->clear();
    mixerElement->clear();
    
    uiState.trackCount = engine.getAllTracks().size();
    for (auto& t : engine.getAllTracks()) {
        if (t->getName() == "Master") {
            uiState.masterTrack.pan = t->getPan();
            uiState.masterTrack.volume = t->getVolume();
            uiState.masterTrack.name = t->getName();
            continue;
        }
        uiState.tracks[t->getName()].clips = t->getClips();
        uiState.tracks[t->getName()].name = t->getName();
        uiState.tracks[t->getName()].volume = t->getVolume();
        uiState.tracks[t->getName()].pan = t->getPan();
        timelineElement->addElements({
            spacer(Modifier().setfixedHeight(2).align(Align::TOP)),
            track(t->getName(), Align::TOP | Align::LEFT),
        });
        mixerElement->addElements({
            spacer(Modifier().setfixedWidth(2).align(Align::LEFT)),
            mixerTrack(t->getName(), Align::TOP | Align::LEFT, decibelsToFloat(t->getVolume()), t->getPan()),
        });
        getSlider(t->getName() + "_volume_slider")->setValue(decibelsToFloat(t->getVolume()));
        getSlider(t->getName() + "_mixer_volume_slider")->setValue(decibelsToFloat(t->getVolume()));
    }
}

void Application::undo() {
    if (undoStack.size() > 1) {
        redoStack.push(undoStack.top());
        undoStack.pop();
        if (!undoStack.empty()) {
            std::string previousState = undoStack.top();
            engine.loadState(previousState);
            rebuildUIFromEngine();
        } 
        else
            std::cout << "No more states to undo." << std::endl;
    } 
    else
        std::cout << "Nothing to undo." << std::endl;
}

void Application::redo() {
    if (!redoStack.empty()) {
        std::string nextState = redoStack.top();
        redoStack.pop();
        engine.loadState(nextState);
        rebuildUIFromEngine();
        undoStack.push(nextState);
    } 
    else
        std::cout << "Nothing to redo." << std::endl;
}

float Application::getDistance(sf::Vector2f point1, sf::Vector2f point2) {
    return sqrt(pow(point2.x - point1.x, 2) + pow(point2.y - point1.y, 2));
}