#include "Application.hpp"

Application::Application() {
    // Initialize window and engine
    screenResolution = sf::VideoMode::getDesktopMode();

    screenResolution.size.x = screenResolution.size.x / 1.5f;
    screenResolution.size.y = screenResolution.size.y / 1.5f;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;

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

    // Initialize project settings values
    projectNameValue = engine.getCurrentCompositionName();
    bpmValue = std::to_string(static_cast<int>(engine.getBpm()));

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
    settingsColumnElement       = settingsColumn();
    dropdownMenu                = generateDropdown({0, 0}, {"Default", "Dark", "Light", "Cyberpunk", "Forest"});
    sampleRateDropdownMenu      = generateSampleRateDropdown({0, 0}, {"44100", "48000", "88200", "96000"});

    dropdownMenu->hide();
    sampleRateDropdownMenu->hide();
    timelineElement->setScrollSpeed(20.f);
    mixerElement->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(not_muted_color),
        contains {
            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "options" << std::endl;}), 
                ButtonStyle::Rect, "Options", resources.dejavuSansFont, primary_text_color, "cm_options"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "rename" << std::endl;}), 
                ButtonStyle::Rect, "Rename", resources.dejavuSansFont, primary_text_color, "cm_rename"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "change color" << std::endl;}), 
                ButtonStyle::Rect, "Change Color", resources.dejavuSansFont, primary_text_color, "cm_change_color"),
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
            contextMenu,
            dropdownMenu,
            sampleRateDropdownMenu
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
            contextMenu,
            dropdownMenu,
            sampleRateDropdownMenu
        }), "mixer" }
    );

    ui->addPage({
        page({
            column(
                Modifier(),
            contains{
                topRowElement,

                row(
                    Modifier().setColor(master_track_color),
                contains{
                    settingsColumnElement
                }),
            }),
            contextMenu,
            dropdownMenu,
            sampleRateDropdownMenu
        }), "settings" }
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
        
        // Handle file tree rebuild if needed
        if (fileTreeNeedsRebuild) {
            buildFileTreeUI();
            fileTreeNeedsRebuild = false;
            shouldForceUpdate = true;
        }
        
        // Handle text input keyboard events (SFML 3.0)
        if (textInputActive || projectNameInputActive || bpmInputActive) {
            // Determine which input is active and get references to the appropriate variables
            bool* activeInput;
            std::string* inputValue;
            std::string textElementId;
            std::string containerElementId;
            
            if (textInputActive) {
                activeInput = &textInputActive;
                inputValue = &textInputValue;
                textElementId = "text_input_box";
                containerElementId = "text_input_row";
            } else if (projectNameInputActive) {
                activeInput = &projectNameInputActive;
                inputValue = &projectNameValue;
                textElementId = "project_name_box";
                containerElementId = "project_name_row";
            } else { // bpmInputActive
                activeInput = &bpmInputActive;
                inputValue = &bpmValue;
                textElementId = "bpm_box";
                containerElementId = "bpm_row";
            }
            
            // Simple text input handling with static variables to track key states
            static bool wasBackspacePressed = false;
            static bool wasEnterPressed = false;
            static bool wasEscapePressed = false;
            static bool wasSpacePressed = false;
            static std::map<sf::Keyboard::Key, bool> keyStates;
            
            // Check for backspace
            bool backspacePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Backspace);
            if (backspacePressed && !wasBackspacePressed) {
                if (!inputValue->empty()) {
                    inputValue->pop_back();
                    shouldForceUpdate = true;
                }
            }
            wasBackspacePressed = backspacePressed;
            
            // Check for Enter or Escape to exit text input mode
            bool enterPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
            bool escapePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
            if ((enterPressed && !wasEnterPressed) || (escapePressed && !wasEscapePressed)) {
                *activeInput = false;
                shouldForceUpdate = true;
                
                // Apply changes if it was project name or BPM
                if (inputValue == &projectNameValue) {
                    engine.setCurrentCompositionName(*inputValue);
                } else if (inputValue == &bpmValue) {
                    try {
                        float bpm = std::stof(*inputValue);
                        if (bpm > 0 && bpm <= 300) { // Reasonable BPM range
                            engine.setBpm(bpm);
                        } else {
                            *inputValue = std::to_string(engine.getBpm()); // Reset to current value if invalid
                        }
                    } catch (...) {
                        *inputValue = std::to_string(engine.getBpm()); // Reset to current value if invalid
                    }
                }
            }
            wasEnterPressed = enterPressed;
            wasEscapePressed = escapePressed;
            
            // Handle letters a-z (skip for BPM input to only allow numbers)
            if (inputValue != &bpmValue) {
                bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                for (int i = 0; i < 26; i++) {
                    sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::A) + i);
                    bool isPressed = sf::Keyboard::isKeyPressed(key);
                    if (isPressed && !keyStates[key]) {
                        char c = 'a' + i;
                        if (shift) c = 'A' + i;
                        *inputValue += c;
                        shouldForceUpdate = true;
                    }
                    keyStates[key] = isPressed;
                }
                
                // Handle space (not for BPM)
                bool spacePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
                if (spacePressed && !wasSpacePressed) {
                    *inputValue += ' ';
                    shouldForceUpdate = true;
                }
                wasSpacePressed = spacePressed;
            }
            
            // Handle numbers 0-9
            for (int i = 0; i <= 9; i++) {
                sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Key::Num0) + i);
                bool isPressed = sf::Keyboard::isKeyPressed(key);
                if (isPressed && !keyStates[key]) {
                    *inputValue += ('0' + i);
                    shouldForceUpdate = true;
                }
                keyStates[key] = isPressed;
            }
            
            // Handle decimal point for BPM
            if (inputValue == &bpmValue) {
                static bool wasPeriodPressed = false;
                bool periodPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Period);
                if (periodPressed && !wasPeriodPressed && inputValue->find('.') == std::string::npos) {
                    *inputValue += '.';
                    shouldForceUpdate = true;
                }
                wasPeriodPressed = periodPressed;
            }
            
            // Check for mouse click outside text input box
            static bool wasMousePressed = false;
            bool mousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
            if (mousePressed && !wasMousePressed) {
                sf::Vector2f mousePos = ui->getMousePosition();
                auto* inputRow = containers.count(containerElementId) ? containers[containerElementId] : nullptr;
                if (inputRow && !inputRow->m_bounds.getGlobalBounds().contains(mousePos)) {
                    *activeInput = false;
                    shouldForceUpdate = true;
                    
                    // Apply changes if it was project name or BPM
                    if (inputValue == &projectNameValue) {
                        engine.setCurrentCompositionName(*inputValue);
                    } else if (inputValue == &bpmValue) {
                        try {
                            float bpm = std::stof(*inputValue);
                            if (bpm > 0 && bpm <= 300) { // Reasonable BPM range
                                engine.setBpm(bpm);
                            } else {
                                *inputValue = std::to_string(engine.getBpm()); // Reset to current value if invalid
                            }
                        } catch (...) {
                            *inputValue = std::to_string(engine.getBpm()); // Reset to current value if invalid
                        }
                    }
                }
            }
            wasMousePressed = mousePressed;
            
            // Update the text display
            if (texts.count(textElementId)) {
                texts[textElementId]->setString(*inputValue);
            }
            
            // Block all other input while text input is active
            if (shouldForceUpdate) {
                ui->forceUpdate();
            }
            return;
        }
        
        // If user interaction, force update
        shouldForceUpdate |= handleContextMenu();
        shouldForceUpdate |= handleUIButtons();
        shouldForceUpdate |= handlePlaybackControls();
        shouldForceUpdate |= handleTrackEvents();
        shouldForceUpdate |= handleKeyboardShortcuts();
        shouldForceUpdate |= handleScrollWheel();
        shouldForceUpdate |= playing;

        // Update UILO ui
        if (shouldForceUpdate) ui->forceUpdate();
        else ui->update(windowView);

        // Update Custom UI Elements (rendered on top of UILO)
        if (!engine.getAllTracks().empty()) {
            float newMasterOffset = timelineOffset;

            for (const auto& track : engine.getAllTracks()) {
                auto* scrollableRow = static_cast<ScrollableRow*>(containers[track->getName() + "_scrollable_row"]);
                scrollableRow->setScrollSpeed(20.f);
                if (scrollableRow->getOffset() != timelineOffset) {
                    newMasterOffset = scrollableRow->getOffset();
                    break;
                }
            }

            if (playing) {
                float playheadXPos = secondsToXPosition(engine.getBpm(), 100.f * uiState.timelineZoomLevel, engine.getPosition());
                float visibleWidth = 0.f;
                
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
                    newMasterOffset = newMasterOffset + (targetOffset - newMasterOffset) * followSpeed;
                }
            }

            float clampedOffset = std::min(0.f, newMasterOffset);

            std::vector<std::shared_ptr<sf::Drawable>> allTimelineElements;

            for (const auto& track : engine.getAllTracks()) {
                auto* trackRow = containers[track->getName() + "_scrollable_row"];
                auto* scrollableRow = static_cast<ScrollableRow*>(trackRow);

                scrollableRow->setScrollSpeed(20.f);
                scrollableRow->setOffset(clampedOffset);

                trackRow->m_modifier.onLClick([&, track, clampedOffset](){
                    sf::Vector2f globalMousePos = ui->getMousePosition();
                    sf::Vector2f trackRowPos = trackRow->getPosition();
                    sf::Vector2f localMousePos = globalMousePos - trackRowPos;
                    
                    auto lines = generateTimelineMeasures(
                        100.f * uiState.timelineZoomLevel,
                        clampedOffset,
                        trackRow->getSize()
                    );
                    
                    float snapX = getNearestMeasureX(localMousePos, lines);
                    float timePosition = xPosToSeconds(engine.getBpm(), 100.f * uiState.timelineZoomLevel, snapX - clampedOffset, clampedOffset);
                    
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
                    
                    std::cout << "Added clip to track '" << track->getName() << "' at time: " << timePosition << " seconds" << std::endl;
                });

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

                float trackYOffset = trackRow->getPosition().y - timelineElement->getPosition().y;
                
                sf::Vector2f timelinePos = timelineElement->getPosition();
                sf::Vector2f trackRowPos = trackRow->getPosition();
                sf::Vector2f scrollableSize = scrollableRow->getSize();
                float scrollableAreaLeft = trackRowPos.x - timelinePos.x;
                float scrollableAreaRight = scrollableAreaLeft + scrollableSize.x;

                auto lines = generateTimelineMeasures(
                    100.f * uiState.timelineZoomLevel,
                    clampedOffset,
                    trackRow->getSize()
                );

                auto clips = generateClipRects(
                    engine.getBpm(), 
                    100.f * uiState.timelineZoomLevel,
                    clampedOffset,
                    trackRow->getSize(),
                    track->getClips()
                );
                
                for (auto& clip : clips) {
                    if (auto rect = std::dynamic_pointer_cast<sf::RectangleShape>(clip)) {
                        sf::Vector2f pos = rect->getPosition();
                        sf::Vector2f size = rect->getSize();
                        
                        pos.y += trackYOffset;
                        
                        float elementRight = pos.x + size.x;
                        if (elementRight > scrollableAreaRight) {
                            size.x = std::max(0.f, scrollableAreaRight - pos.x);
                        }
                        
                        if (pos.x < scrollableAreaLeft) {
                            float clipAmount = scrollableAreaLeft - pos.x;
                            pos.x = scrollableAreaLeft;
                            size.x = std::max(0.f, size.x - clipAmount);
                        }
                        
                        rect->setPosition(pos);
                        rect->setSize(size);
                    }
                }
                
                for (auto& line : lines) {
                    if (auto rect = std::dynamic_pointer_cast<sf::RectangleShape>(line)) {
                        sf::Vector2f pos = rect->getPosition();
                        sf::Vector2f size = rect->getSize();
                        
                        pos.y += trackYOffset;
                        
                        float elementRight = pos.x + size.x;
                        if (elementRight > scrollableAreaRight) {
                            size.x = std::max(0.f, scrollableAreaRight - pos.x);
                        }
                        
                        if (pos.x < scrollableAreaLeft) {
                            float clipAmount = scrollableAreaLeft - pos.x;
                            pos.x = scrollableAreaLeft;
                            size.x = std::max(0.f, size.x - clipAmount);
                        }
                        
                        rect->setPosition(pos);
                        rect->setSize(size);
                    }
                }
                
                allTimelineElements.insert(allTimelineElements.end(), clips.begin(), clips.end());
                allTimelineElements.insert(allTimelineElements.end(), lines.begin(), lines.end());

                std::vector<std::shared_ptr<sf::Drawable>> emptyGeometry;
                static_cast<uilo::Element*>(trackRow)->setCustomGeometry(emptyGeometry);
            }

            auto playhead = getPlayHead(
                engine.getBpm(), 
                100.f * uiState.timelineZoomLevel,
                clampedOffset,
                engine.getPosition(), 
                sf::Vector2f(4.f, (engine.getAllTracks().size() - 1) * 98.f + 96.f)
            );
            
            if (auto playheadRect = std::dynamic_pointer_cast<sf::RectangleShape>(playhead)) {
                if (!engine.getAllTracks().empty()) {
                    auto* firstTrackRow = containers[engine.getAllTracks()[0]->getName() + "_scrollable_row"];
                    if (firstTrackRow) {
                        float firstTrackYOffset = firstTrackRow->getPosition().y - timelineElement->getPosition().y;
                        sf::Vector2f pos = playheadRect->getPosition();
                        pos.y = firstTrackYOffset;
                        playheadRect->setPosition(pos);
                    }
                }
            }
            
            allTimelineElements.push_back(playhead);
            timelineElement->setCustomGeometry(allTimelineElements);
            timelineOffset = clampedOffset;
        }

        if (showThemeDropdown) {
            if (!dropdownMenu->m_modifier.isVisible()) {
                // Position the dropdown below the theme button
                sf::Vector2f themeButtonPos = containers["theme_dropdown"]->getPosition();
                sf::Vector2f themeButtonSize = containers["theme_dropdown"]->getSize();
                sf::Vector2f dropdownPos = {themeButtonPos.x, themeButtonPos.y + themeButtonSize.y + 4};
                
                dropdownMenu->setPosition(dropdownPos);
                dropdownMenu->m_modifier.setfixedWidth(themeButtonSize.x);
                dropdownMenu->show();
                shouldForceUpdate = true;
                std::cout << "Showing theme dropdown at position: " << dropdownPos.x << ", " << dropdownPos.y << std::endl;
            }
        }
        else {
            if (dropdownMenu->m_modifier.isVisible()) {
                dropdownMenu->hide();
                shouldForceUpdate = true;
            }
        }

        if (showSampleRateDropdown) {
            if (!sampleRateDropdownMenu->m_modifier.isVisible()) {
                // Position the dropdown below the sample rate button
                sf::Vector2f sampleRateButtonPos = containers["sample_rate_dropdown"]->getPosition();
                sf::Vector2f sampleRateButtonSize = containers["sample_rate_dropdown"]->getSize();
                sf::Vector2f dropdownPos = {sampleRateButtonPos.x, sampleRateButtonPos.y + sampleRateButtonSize.y + 4};
                
                sampleRateDropdownMenu->setPosition(dropdownPos);
                sampleRateDropdownMenu->m_modifier.setfixedWidth(sampleRateButtonSize.x);
                sampleRateDropdownMenu->show();
                shouldForceUpdate = true;
                std::cout << "Showing sample rate dropdown at position: " << dropdownPos.x << ", " << dropdownPos.y << std::endl;
            }
        }
        else {
            if (sampleRateDropdownMenu->m_modifier.isVisible()) {
                sampleRateDropdownMenu->hide();
                shouldForceUpdate = true;
            }
        }
    }
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

    // Handle dropdown menu clicks outside - only when visible
    if (showThemeDropdown && dropdownMenu->m_modifier.isVisible()) {
        static bool prevLeftClick = false;
        bool leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        
        if (leftClick && !prevLeftClick) {
            sf::Vector2f mousePos = ui->getMousePosition();
            bool clickedInDropdown = dropdownMenu->getBounds().contains(mousePos);
            bool clickedInThemeButton = containers["theme_dropdown"]->m_bounds.getGlobalBounds().contains(mousePos);
            
            if (!clickedInDropdown && !clickedInThemeButton) {
                showThemeDropdown = false;
                shouldForceUpdate = true;
            }
        }
        prevLeftClick = leftClick;
    }

    // Handle sample rate dropdown menu clicks outside - only when visible
    if (showSampleRateDropdown && sampleRateDropdownMenu->m_modifier.isVisible()) {
        static bool prevLeftClickSampleRate = false;
        bool leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        
        if (leftClick && !prevLeftClickSampleRate) {
            sf::Vector2f mousePos = ui->getMousePosition();
            bool clickedInDropdown = sampleRateDropdownMenu->getBounds().contains(mousePos);
            bool clickedInSampleRateButton = containers["sample_rate_dropdown"]->m_bounds.getGlobalBounds().contains(mousePos);
            
            if (!clickedInDropdown && !clickedInSampleRateButton) {
                showSampleRateDropdown = false;
                shouldForceUpdate = true;
            }
        }
        prevLeftClickSampleRate = leftClick;
    }

    if (buttons["select_directory"]->isClicked()) {
        fileTree.setRootDirectory(selectDirectory());
        buildFileTreeUI();
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

    if (buttons["settings"]->isClicked()) {
        showSettings = !showSettings;

        if (showSettings)
            ui->switchToPage("settings");
        else
            ui->switchToPage("timeline");
        
        shouldForceUpdate = true;
    }

    if (buttons["new_track"]->isClicked()) {
        uiChanged = true;
        newTrack(selectFile({"*.wav", "*.mp3", "*.flac"}));
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

    if (ctrl && !prevCtrl) prevZ = prevY = false;

    if (ctrl && z && !prevZ) {
        undo();
        std::cout << "Undo, undoStack size: " << undoStack.size() << std::endl;
        shouldForceUpdate = true;
    }
    if (ctrl && y && !prevY) {
        redo();
        std::cout << "Redo, redoStack size: " << redoStack.size() << std::endl;
        shouldForceUpdate = true;
    }

    prevCtrl = ctrl;
    prevZ = z;
    prevY = y;
    return shouldForceUpdate;
}

bool Application::handleScrollWheel() {
    static bool prevCtrl = false, prevPlus = false, prevMinus = false;
    bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
    bool plus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Equal);
    bool minus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Hyphen);
    bool shouldForceUpdate = false;

    if (ctrl && plus && !prevPlus) {
        const float zoomSpeed = 0.2f;
        const float maxZoom = 5.0f;
        uiState.timelineZoomLevel = std::min(maxZoom, uiState.timelineZoomLevel + zoomSpeed);
        std::cout << "Zoom in: " << uiState.timelineZoomLevel << std::endl;
        shouldForceUpdate = true;
    }
    
    if (ctrl && minus && !prevMinus) {
        const float zoomSpeed = 0.2f;
        const float minZoom = 0.1f;
        uiState.timelineZoomLevel = std::max(minZoom, uiState.timelineZoomLevel - zoomSpeed);
        std::cout << "Zoom out: " << uiState.timelineZoomLevel << std::endl;
        shouldForceUpdate = true;
    }

    prevCtrl = ctrl;
    prevPlus = plus;
    prevMinus = minus;
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
    resources.dejavuSansFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/SpaceMono-Regular.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/SpaceMono-Regular.ttf");
    }
    resources.spaceMonoFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.bold.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.bold.ttf");
    }
    resources.ubuntuBoldFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.mono.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.mono.ttf");
    }
    resources.ubuntuMonoFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.mono-bold.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.mono-bold.ttf");
    }
    resources.ubuntuMonoBoldFont = fontFile.getFullPathName().toStdString();
}

Row* Application::topRow() {
    return row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(64)
            .setColor(foreground_color),
    contains{
        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill, 
            "load", 
            resources.dejavuSansFont, 
            secondary_text_color,
            "load"
        ),

        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "save",
            resources.dejavuSansFont,
            secondary_text_color,
            "save"
        ),

        button(
            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "play",
            resources.dejavuSansFont,
            secondary_text_color,
            "play"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "settings",
            resources.dejavuSansFont,
            secondary_text_color,
            "settings"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "mixer",
            resources.dejavuSansFont,
            secondary_text_color,
            "mixer"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(button_color),
            ButtonStyle::Pill,
            "+ track",
            resources.dejavuSansFont,
            secondary_text_color,
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
                .setColor(middle_color),
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

ScrollableColumn* Application::fileBrowser() {
    return scrollableColumn(
        Modifier()
            .align(Align::LEFT)
            .setfixedWidth(360)
            .setColor(track_color),
    contains{
        spacer(Modifier().setfixedHeight(16).align(Align::TOP)),

        button(
            Modifier()
                .setfixedHeight(48)
                .setWidth(0.8f)
                .setColor(alt_button_color)
                .align(Align::CENTER_X),
            ButtonStyle::Pill,
            "Browse Files",
            resources.dejavuSansFont,
            secondary_text_color,
            "select_directory"
        ),

        spacer(Modifier().setfixedHeight(16)),
    });
}

void Application::buildFileTreeUI() {
    // Clear existing file tree UI
    fileBrowserElement->clear();
    
    // Re-add the button and spacer
    fileBrowserElement->addElements({
        spacer(Modifier().setfixedHeight(16).align(Align::TOP)),

        button(
            Modifier()
                .setfixedHeight(48)
                .setWidth(0.8f)
                .setColor(alt_button_color)
                .align(Align::CENTER_X),
            ButtonStyle::Pill,
            "Browse Files",
            resources.dejavuSansFont,
            secondary_text_color,
            "select_directory"
        ),

        spacer(Modifier().setfixedHeight(16)),
    });
    
    // Add root directory with 20px indentation
    std::string displayName = fileTree.getName();
    if (fileTree.isDirectory()) {
        std::string symbol = "[d] ";
        displayName = symbol + displayName;
    }
    
    auto rootTextElement = text(
        Modifier().setfixedHeight(28).setColor(primary_text_color), 
        displayName, 
        resources.dejavuSansFont
    );
    
    if (fileTree.isDirectory()) {
        rootTextElement->m_modifier.onLClick([&](){
            fileTree.toggleOpen();
            fileTreeNeedsRebuild = true;
        });
    }
    
    fileBrowserElement->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(20.f)),
            rootTextElement,
        }),
        spacer(Modifier().setfixedHeight(12))
    });
    
    // Only add children if root directory is open
    if (fileTree.isOpen()) {
        for (const auto& subDir : fileTree.getSubDirectories()) {
            buildFileTreeUIRecursive(*subDir, 2);
        }
        
        for (const auto& file : fileTree.getFiles()) {
            buildFileTreeUIRecursive(*file, 2);
        }
    }
}

void Application::buildFileTreeUIRecursive(const FileTree& tree, int indentLevel) {
    float indent = indentLevel * 20.f;
    
    // Add current directory/file
    std::string displayName = tree.getName();
    if (tree.isDirectory()) {
        std::string symbol = "[d] ";
        displayName = symbol + displayName;
    } else if (tree.isAudioFile()) {
        displayName = displayName;
    }
    
    auto textElement = text(
        Modifier().setfixedHeight(28).setColor(primary_text_color), 
        displayName, 
        resources.dejavuSansFont
    );
    
    // If it's a directory, add click handler to toggle open/close
    if (tree.isDirectory()) {
        std::string treePath = tree.getPath();
        textElement->m_modifier.onLClick([this, treePath](){
            toggleTreeNodeByPath(treePath);
            fileTreeNeedsRebuild = true;
        });
    }
    // If it's an audio file, add click handler to create new track
    else if (tree.isAudioFile()) {
        std::string filePath = tree.getPath();
        textElement->m_modifier.onLClick([this, filePath](){
            newTrack(filePath);
        });
    }
    
    fileBrowserElement->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(indent)),
            textElement,
        }),
        spacer(Modifier().setfixedHeight(12))
    });
    
    // Only add children if this directory is open
    if (tree.isDirectory() && tree.isOpen()) {
        for (const auto& subDir : tree.getSubDirectories()) {
            buildFileTreeUIRecursive(*subDir, indentLevel + 1);
        }
        
        for (const auto& file : tree.getFiles()) {
            buildFileTreeUIRecursive(*file, indentLevel + 1);
        }
    }
}

void Application::toggleTreeNodeByPath(const std::string& path) {
    std::function<bool(FileTree&)> findAndToggle = [&](FileTree& node) -> bool {
        if (node.getPath() == path) {
            node.toggleOpen();
            return true;
        }
        
        for (const auto& subDir : node.getSubDirectories()) {
            if (findAndToggle(*subDir)) {
                return true;
            }
        }
        
        for (const auto& file : node.getFiles()) {
            if (findAndToggle(*file)) {
                return true;
            }
        }
        
        return false;
    };
    
    findAndToggle(fileTree);
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
            .setColor(foreground_color)
            .align(Align::BOTTOM),
    contains{
        
    });
}

Row* Application::track(const std::string& trackName, Align alignment, float volume, float pan) {
    std::cout << "Creating track: " << trackName << std::endl;
    return row(
        Modifier()
            .setColor(track_row_color)
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
                        Modifier().setColor(primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        trackName,
                        resources.dejavuSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(not_muted_color),
                            ButtonStyle::Rect,
                            "mute",
                            resources.dejavuSansFont,
                            secondary_text_color,
                            "mute_" + trackName
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    slider_knob_color,
                    slider_bar_color,
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
            Modifier().setColor(primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
            trackName,
            resources.dejavuSansFont
        ),

        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        slider(
            Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
            slider_knob_color,
            slider_bar_color,
            trackName + "_mixer_volume_slider"
        ),
        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        button(
            Modifier()
                .setfixedHeight(32)
                .setfixedWidth(64)
                .align(Align::CENTER_X | Align::BOTTOM)
                .setColor(button_color),
            ButtonStyle::Rect,
            "solo",
            resources.dejavuSansFont,
            secondary_text_color,
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
            Modifier().setColor(primary_text_color).setfixedHeight(18).align(Align::CENTER_X | Align::TOP),
            trackName,
            resources.dejavuSansFont
        ),

        spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

        slider(
            Modifier().setfixedWidth(32).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
            slider_knob_color,
            slider_bar_color,
            "Master_mixer_volume_slider"
        ),
        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        button(
            Modifier()
                .setfixedHeight(32)
                .setfixedWidth(64)
                .align(Align::CENTER_X | Align::BOTTOM)
                .setColor(button_color),
            ButtonStyle::Rect,
            "solo",
            resources.dejavuSansFont,
            secondary_text_color,
            "solo_Master"
        ),
    });
}

Row* Application::masterTrack() {
    return row(
        Modifier()
            .setColor(track_row_color)
            .setfixedHeight(96)
            .align(Align::LEFT | Align::BOTTOM),
    contains{
        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(master_track_color),
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
                        Modifier().setColor(primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        "Master",
                        resources.dejavuSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(not_muted_color),
                            ButtonStyle::Rect,
                            "mute",
                            resources.dejavuSansFont,
                            secondary_text_color,
                            "mute_Master"
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    slider_knob_color,
                    slider_bar_color,
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
        Modifier().setWidth(1.f).setHeight(1.f).setColor(track_row_color),
    contains{
       
    }, "mixer");
}

ScrollableColumn* Application::settingsColumn() {
    return scrollableColumn(
        Modifier().setfixedWidth(1024.f).setColor(track_color).align(Align::CENTER_X),
    contains{
        spacer(Modifier().setfixedHeight(32.f)),
        text(
            Modifier().setfixedHeight(48).setColor(primary_text_color).align(Align::LEFT),
            "  UI",
            resources.dejavuSansFont,
            "ui_section_text"
        ),
        spacer(Modifier().setfixedHeight(8.f)),
        row(Modifier().setfixedHeight(32), contains{
            spacer(Modifier().setfixedWidth(32.f)),
            text(Modifier().setfixedHeight(32).setColor(primary_text_color).align(Align::LEFT | Align::CENTER_Y), "Select Theme", resources.dejavuSansFont, "select_theme_text"),
            row(Modifier().setfixedHeight(32).setfixedWidth(256).align(Align::RIGHT).setColor(sf::Color::White).onLClick([&](){
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible()) {
                    showThemeDropdown = !showThemeDropdown;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), "Default", resources.dejavuSansFont, "theme_text"),
            }, "theme_dropdown"),
            spacer(Modifier().setfixedWidth(32.f).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(64.f)),
        text(
            Modifier().setfixedHeight(48).setColor(primary_text_color).align(Align::LEFT),
            "  Audio",
            resources.dejavuSansFont,
            "audio_section_text"
        ),
        spacer(Modifier().setfixedHeight(16.f)),
        row(Modifier().setfixedHeight(32), contains{
            spacer(Modifier().setfixedWidth(32.f)),
            text(Modifier().setfixedHeight(32).setColor(primary_text_color).align(Align::LEFT | Align::CENTER_Y), "Sample Rate", resources.dejavuSansFont, "select_sample_rate_text"),
            row(Modifier().setfixedHeight(32).setfixedWidth(256).align(Align::RIGHT).setColor(sf::Color::White).onLClick([&](){
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible()) {
                    showSampleRateDropdown = !showSampleRateDropdown;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), "44100", resources.dejavuSansFont, "sample_rate_text"),
            }, "sample_rate_dropdown"),
            spacer(Modifier().setfixedWidth(32.f).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(64.f)),
        text(
            Modifier().setfixedHeight(48).setColor(primary_text_color).align(Align::LEFT),
            "  Project",
            resources.dejavuSansFont,
            "project_section_text"
        ),
        spacer(Modifier().setfixedHeight(16.f)),
        // Project name input
        row(Modifier().setfixedHeight(32), contains{
            spacer(Modifier().setfixedWidth(32.f)),
            text(Modifier().setfixedHeight(32).setColor(primary_text_color).align(Align::LEFT | Align::CENTER_Y), "Project Name", resources.dejavuSansFont, "project_name_label"),
            row(Modifier().setfixedHeight(32).setfixedWidth(256).align(Align::RIGHT).setColor(sf::Color::White).onLClick([&](){
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible() && !textInputActive && !bpmInputActive) {
                    projectNameInputActive = true;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), projectNameValue, resources.dejavuSansFont, "project_name_box"),
            }, "project_name_row"),
            spacer(Modifier().setfixedWidth(32.f).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(16.f)),
        // BPM input
        row(Modifier().setfixedHeight(32), contains{
            spacer(Modifier().setfixedWidth(32.f)),
            text(Modifier().setfixedHeight(32).setColor(primary_text_color).align(Align::LEFT | Align::CENTER_Y), "BPM", resources.dejavuSansFont, "bpm_label"),
            row(Modifier().setfixedHeight(32).setfixedWidth(256).align(Align::RIGHT).setColor(sf::Color::White).onLClick([&](){
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible() && !textInputActive && !projectNameInputActive) {
                    bpmInputActive = true;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), bpmValue, resources.dejavuSansFont, "bpm_box"),
            }, "bpm_row"),
            spacer(Modifier().setfixedWidth(32.f).align(Align::RIGHT)),
        })
    });
}

FreeColumn* Application::generateDropdown(sf::Vector2f position, const std::vector<std::string>& items) {
    // Calculate height based on number of items
    float itemHeight = 32.f;
    float spacerHeight = 1.f;
    float totalHeight = items.size() * itemHeight + (items.size() - 1) * spacerHeight;
    
    // Create a temporary freeColumn first, then we'll dynamically add elements
    auto* dropdown = freeColumn(
        Modifier()
            .setfixedHeight(totalHeight)
            .setfixedWidth(200)
            .setColor(not_muted_color)
    );
    
    // Add elements one by one
    for (size_t i = 0; i < items.size(); ++i) {
        // Add the button for this item
        dropdown->addElement(
            button(
                Modifier()
                    .setfixedHeight(itemHeight)
                    .setColor(sf::Color::White)
                    .onLClick([this, item = items[i]](){
                        // Only process clicks if dropdown is visible
                        if (!showThemeDropdown || !dropdownMenu->m_modifier.isVisible()) {
                            return;
                        }
                        
                        static std::string lastSelected = "";
                        static auto lastClickTime = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime);
                        
                        // Prevent rapid clicking and duplicate selections
                        if (item == lastSelected && timeDiff.count() < 100) {
                            return;
                        }
                        
                        std::cout << "Selected: " << item << std::endl;
                        
                        // Apply the selected theme
                        if (item == "Default") {
                            applyTheme(Themes::Default);
                        } else if (item == "Dark") {
                            applyTheme(Themes::Dark);
                        } else if (item == "Light") {
                            applyTheme(Themes::Light);
                        } else if (item == "Cyberpunk") {
                            applyTheme(Themes::Cyberpunk);
                        } else if (item == "Forest") {
                            applyTheme(Themes::Forest);
                        }
                        
                        // Update the theme text in the settings
                        if (texts.count("theme_text")) {
                            texts["theme_text"]->setString(item);
                        }
                        
                        showThemeDropdown = false; // Close dropdown after selection
                        lastSelected = item;
                        lastClickTime = now;
                    }), 
                ButtonStyle::Rect, 
                items[i], 
                resources.dejavuSansFont, 
                sf::Color::Black, 
                "dropdown_item_" + std::to_string(i)
            )
        );
        
        // Add spacer between items (except after the last one)
        if (i < items.size() - 1) {
            dropdown->addElement(
                spacer(Modifier().setfixedHeight(spacerHeight))
            );
        }
    }
    
    // Set the position
    dropdown->setPosition(position);
    
    return dropdown;
}

FreeColumn* Application::generateSampleRateDropdown(sf::Vector2f position, const std::vector<std::string>& items) {
    // Calculate height based on number of items
    float itemHeight = 32.f;
    float spacerHeight = 1.f;
    float totalHeight = items.size() * itemHeight + (items.size() - 1) * spacerHeight;
    
    // Create a temporary freeColumn first, then we'll dynamically add elements
    auto* dropdown = freeColumn(
        Modifier()
            .setfixedHeight(totalHeight)
            .setfixedWidth(200)
            .setColor(not_muted_color)
    );
    
    // Add elements one by one
    for (size_t i = 0; i < items.size(); ++i) {
        // Add the button for this item
        dropdown->addElement(
            button(
                Modifier()
                    .setfixedHeight(itemHeight)
                    .setColor(sf::Color::White)
                    .onLClick([this, item = items[i]](){
                        // Only process clicks if dropdown is visible
                        if (!showSampleRateDropdown || !sampleRateDropdownMenu->m_modifier.isVisible()) {
                            return;
                        }
                        
                        static std::string lastSelected = "";
                        static auto lastClickTime = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime);
                        
                        // Prevent rapid clicking and duplicate selections
                        if (item == lastSelected && timeDiff.count() < 100) {
                            return;
                        }
                        
                        std::cout << "Selected sample rate: " << item << " Hz" << std::endl;
                        
                        // Note: Sample rate changes require audio device restart
                        // For now, just update the display
                        // TODO: Implement sample rate changing functionality in Engine
                        
                        // Update the sample rate text in the settings
                        if (texts.count("sample_rate_text")) {
                            texts["sample_rate_text"]->setString(item);
                        }
                        
                        showSampleRateDropdown = false; // Close dropdown after selection
                        lastSelected = item;
                        lastClickTime = now;
                    }), 
                ButtonStyle::Rect, 
                items[i] + " Hz", 
                resources.dejavuSansFont, 
                sf::Color::Black, 
                "sample_rate_item_" + std::to_string(i)
            )
        );
        
        // Add spacer between items (except after the last one)
        if (i < items.size() - 1) {
            dropdown->addElement(
                spacer(Modifier().setfixedHeight(spacerHeight))
            );
        }
    }
    
    // Set the position
    dropdown->setPosition(position);
    
    return dropdown;
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

void Application::newTrack(const std::string& samplePath) {
    if (samplePath.empty()) return;

    std::string trackName;
    int trackIndex = uiState.trackCount;

    if (!samplePath.empty()) {
        juce::File sampleFile(samplePath);
        trackName = sampleFile.getFileNameWithoutExtension().toStdString();
        engine.addTrack(trackName);

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
    
    // Update project settings values
    projectNameValue = engine.getCurrentCompositionName();
    bpmValue = std::to_string(static_cast<int>(engine.getBpm()));
}

bool Application::handleTrackEvents() {
    bool shouldForceUpdate = false;

    if (getButton("mute_Master")->isClicked()) {
        engine.getMasterTrack()->toggleMute();
        getButton("mute_Master")->m_modifier.setColor((engine.getMasterTrack()->isMuted() ? mute_color : not_muted_color));
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
            getButton("mute_" + track->getName())->m_modifier.setColor((track->isMuted() ? mute_color : not_muted_color));
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