#include "Application.hpp"

// OLD APPLICATION

Application::Application() {
    // Initialize auto-save timer from config
    loadConfig();
    applyThemeByName(selectedThemeName); // Apply the theme from config
    autoSaveTimer.restart();
    uiState.autoSaveIntervalSeconds = autoSaveIntervalSeconds;

    // 2) Window & engine setup
    screenResolution = sf::VideoMode::getDesktopMode();
    screenResolution.size.x /= 1.5f;
    screenResolution.size.y /= 1.5f;
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;

    windowView.setSize({ (float)screenResolution.size.x, (float)screenResolution.size.y });
    windowView.setCenter({ (float)screenResolution.size.x/2.f, (float)screenResolution.size.y/2.f });
    window.create(screenResolution, "MULO", sf::Style::Default, sf::State::Windowed, settings);
    window.setVerticalSyncEnabled(true);

    engine.newComposition("untitled");
    engine.addTrack("Master");
    
    // Set master track pan to center
    if (engine.getMasterTrack()) {
        engine.getMasterTrack()->setPan(0.5f);
    }
    
    initUIResources();

    // Initialize project settings values
    projectNameValue = engine.getCurrentCompositionName();
    bpmValue = std::to_string(static_cast<int>(engine.getBpm()));
    autosaveValue = std::to_string(autoSaveIntervalSeconds);

    timelineComponent.setAppRef(this);
    timelineComponent.setEngineRef(&engine);
    timelineComponent.setUIStateRef(&uiState);
    timelineComponent.setResourcesRef(&resources);

    // UI Elements
    topRowElement               = topRow();
    fileBrowserElement          = fileBrowser();

    masterTrackElement          = masterTrack();
    timelineElement             = timelineComponent.getLayout();

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
    // timelineElement->setScrollSpeed(20.f);
    mixerElement   ->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(not_muted_color),
        contains {
            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "options" << std::endl;}), 
                ButtonStyle::Rect, "Options", resources.dejavuSansFont, black, "cm_options"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "rename" << std::endl;}), 
                ButtonStyle::Rect, "Rename", resources.dejavuSansFont, black, "cm_rename"),
            spacer(Modifier().setfixedHeight(1)),

            button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "change color" << std::endl;}), 
                ButtonStyle::Rect, "Change Color", resources.dejavuSansFont, black, "cm_change_color"),
            spacer(Modifier().setfixedHeight(1)),
        }
    );
    contextMenu->hide();

    toolTip = freeColumn(
        Modifier().setfixedHeight(32).setfixedWidth(750).setColor(sf::Color::Transparent),
        contains {
            text(Modifier().setfixedHeight(28).align(Align::CENTER_Y).setColor(primary_text_color), "Test string", resources.dejavuSansFont, "tool_tip"),
        }
    );
    toolTip->hide();

    // 4) Build pages
    ui = new UILO(window, windowView, {{
        page({
            column(
                Modifier(),
            contains{
                topRowElement,
                browserAndTimelineElement,
                fxRackElement,
            }),
            toolTip,
            contextMenu,
            dropdownMenu,
            sampleRateDropdownMenu
        }), "timeline" }
    });
    ui->addPage({
        page({
            column(
                Modifier(),
            contains{
                topRowElement,
                browserAndMixerElement,
                fxRackElement,
            }),
            toolTip,
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
            toolTip,
            contextMenu,
            dropdownMenu,
            sampleRateDropdownMenu
        }), "settings" }
    );

    running = ui->isRunning();

    ui->switchToPage("timeline");
    loadComposition("assets/empty_project.mpf");
    
    // Set pan sliders to center position (0.5f)
    if (getSlider("Master_mixer_pan_slider")) {
        getSlider("Master_mixer_pan_slider")->setValue(0.5f);
    }
    
    ui->forceUpdate();
}

Application::~Application(){
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
        
        // If user interaction, force update
        shouldForceUpdate |= handleContextMenu();
        shouldForceUpdate |= handleToolTips();
        shouldForceUpdate |= handleUIButtons();
        timelineComponent.handleEvents();
        shouldForceUpdate |= handleKeyboardShortcuts();
        shouldForceUpdate |= handleTextInput();
        shouldForceUpdate |= handleScrollWheel();
        shouldForceUpdate |= playing;

        // Check auto save
        checkAutoSave();

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

bool Application::handleToolTips() {
    std::string toolTipMessage = "";
    std::string hoveredButtonId = "";

    // Check which button is currently being hovered
    if(getButton("select_directory")->isHovered()) {
        toolTipMessage = "Press this button to set your preferred directory for your tracks.";
        hoveredButtonId = "select_directory";
    }
    else if(getButton("mute_Master")->isHovered()) {
        toolTipMessage = "Press this to mute the entire composition.";
        hoveredButtonId = "mute_Master";
    }
    else if(getButton("mixer")->isHovered()) {
        toolTipMessage = "Press this button to switch between timeline and mixer.";
        hoveredButtonId = "mixer";
    }
    else if(getButton("play")->isHovered()) {
        toolTipMessage = "Press this to play your composition.";
        hoveredButtonId = "play";
    }
    else if(getButton("save")->isHovered()) {
        toolTipMessage = "Press this to manually save the current state of your composition.";
        hoveredButtonId = "save";
    }
    else if(getButton("load")->isHovered()) {
        toolTipMessage = "Press this to load another composition.";
        hoveredButtonId = "load";
    }

    // Handle tooltip timer logic
    if (!hoveredButtonId.empty()) {
        if (currentHoveredButton != hoveredButtonId) {
            currentHoveredButton = hoveredButtonId;
            toolTipTimer.restart();
            tooltipShown = false;
            toolTip->hide();
        }
        
        if (toolTipTimer.getElapsedTime().asSeconds() >= 1.5f && !tooltipShown) {
            tooltipShown = true;
            
            sf::Vector2f mousePos = ui->getMousePosition();
            const float tooltipWidth = toolTip->getSize().x;
            const float tooltipHeight = 32.f;
            const float offset = 20.f;
            
            float tooltipX = mousePos.x + offset;
            float tooltipY = mousePos.y + offset;
            
            if (tooltipX + tooltipWidth > screenResolution.size.x) {
                tooltipX = mousePos.x - tooltipWidth - offset;
            }
            
            if (tooltipY + tooltipHeight > screenResolution.size.y) {
                tooltipY = mousePos.y - tooltipHeight - offset;
            }
            
            tooltipX = std::max(0.f, tooltipX);
            tooltipY = std::max(0.f, tooltipY);
            
            toolTip->setPosition({tooltipX, tooltipY});
            toolTip->show();
            if (getText("tool_tip")) {
                getText("tool_tip")->setString(toolTipMessage);
            }
            return true;
        }
        
        return false;
    }
    else {
        if (tooltipShown || !currentHoveredButton.empty()) {
            currentHoveredButton = "";
            tooltipShown = false;
            toolTip->hide();
            return true;
        }
        return false;
    }
}

bool Application::handleUIButtons() {
    shouldForceUpdate = false;

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

    return shouldForceUpdate;
}

bool Application::handleKeyboardShortcuts() {
    static bool prevCtrl = false, prevZ = false, prevY = false, prevSpace = false;
    bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
    bool z = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z);
    bool y = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Y);
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool shouldForceUpdate = false;

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

    // Handle Space (Play/Pause)
    if (space && !prevSpace) {
        if (auto* playButton = getButton("play")) {
            // Call the play button's click handler directly
            playButton->m_modifier.getOnLClick()();
            shouldForceUpdate = true;
        }
    }

    prevCtrl = ctrl;
    prevZ = z;
    prevY = y;
    prevSpace = space;
    return shouldForceUpdate;
}

bool Application::handleTextInput() {
    if (!textInputActive && !projectNameInputActive && !bpmInputActive && !autosaveInputActive) {
        return false;
    }
    
    bool shouldForceUpdate = false;
    
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
    } else if (bpmInputActive) {
        activeInput = &bpmInputActive;
        inputValue = &bpmValue;
        textElementId = "bpm_box";
        containerElementId = "bpm_row";
    } else { // autosaveInputActive
        activeInput = &autosaveInputActive;
        inputValue = &autosaveValue;
        textElementId = "autosave_box";
        containerElementId = "autosave_row";
        static bool debugShown = false;
        if (!debugShown) {
            std::cout << "Autosave input is active, current value: " << *inputValue << std::endl;
            debugShown = true;
        }
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
        
        // Apply changes if it was project name, BPM, or autosave
        applyTextInputChanges(inputValue);
    }
    wasEnterPressed = enterPressed;
    wasEscapePressed = escapePressed;
    
    // Handle letters a-z (skip for BPM and autosave input to only allow numbers)
    if (inputValue != &bpmValue && inputValue != &autosaveValue) {
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
            
            // Apply changes if it was project name, BPM, or autosave
            applyTextInputChanges(inputValue);
        }
    }
    wasMousePressed = mousePressed;
    
    // Update the text display
    if (texts.count(textElementId)) {
        texts[textElementId]->setString(*inputValue);
    }
    
    return shouldForceUpdate;
}

void Application::applyTextInputChanges(std::string* inputValue) {
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
    } else if (inputValue == &autosaveValue) {
        try {
            int interval = std::stoi(*inputValue);
            if (interval >= 10 && interval <= 3600) { // 10 seconds to 1 hour
                autoSaveIntervalSeconds = interval;
                uiState.autoSaveIntervalSeconds = interval;
                autoSaveTimer.restart(); // Restart timer with new interval
                saveConfig(); // Save to config file
                std::cout << "Auto-save interval updated to " << interval << " seconds" << std::endl;
            } else {
                *inputValue = std::to_string(autoSaveIntervalSeconds); // Reset to current value if invalid
                std::cout << "Invalid auto-save interval: " << *inputValue << " (must be between 10 and 3600 seconds)" << std::endl;
            }
        } catch (...) {
            *inputValue = std::to_string(autoSaveIntervalSeconds); // Reset to current value if invalid
            std::cout << "Invalid auto-save interval format: " << *inputValue << std::endl;
        }
    }
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
        ui->render();
        window.display();
    }
}

bool Application::isRunning() const {
    return running;
}

void Application::initUIResources() {
    juce::File fontFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile("assets/fonts/DejaVuSans.ttf");
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

        // Load
        button(
            Modifier()
                .align(Align::LEFT | Align::CENTER_Y)
                .setHeight(.75f)
                .setfixedWidth(96)
                .setColor(button_color)
                .onLClick([&](){
                    std::string path = selectFile({"*.mpf"});
                    if (!path.empty())
                        loadComposition(path);
                }),
            ButtonStyle::Pill, 
            "load", 
            resources.dejavuSansFont, 
            secondary_text_color,
            "load"
        ),
        spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

        // Save
        button(
            Modifier()
                .align(Align::LEFT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color)
                .onLClick([&](){
                    uiState.saveDirectory = selectDirectory();
                    if (engine.saveState(uiState.saveDirectory + "/" + engine.getCurrentCompositionName() + ".mpf"))
                        std::cout << "Project saved successfully." << std::endl;
                    else
                        std::cerr << "Failed to save project." << std::endl;
                }),
            ButtonStyle::Pill,
            "save",
            resources.dejavuSansFont,
            secondary_text_color,
            "save"
        ),
        spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

        // Play/Pause
        button(
            Modifier()
                .align(Align::CENTER_X | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color)
                .onLClick([&](){
                    playing = !playing;
                    if (playing) {
                        std::cout << "Playing audio..." << std::endl;
                        engine.play();
                        getButton("play")->setText("pause");
                    } else {
                        std::cout << "Pausing audio..." << std::endl;
                        engine.pause();
                        engine.setPosition(0.0);
                        getButton("play")->setText("play");
                    }
                    shouldForceUpdate = true;
                }),
            ButtonStyle::Pill,
            "play",
            resources.dejavuSansFont,
            secondary_text_color,
            "play"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier()
                .align(Align::RIGHT | Align::CENTER_Y)
                .setHeight(.75f)
                .setfixedWidth(96)
                .setColor(button_color)
                .onLClick([&](){
                    showSettings = !showSettings;
                    const std::string pageToShow = showSettings ? "settings" : "timeline";
                    ui->switchToPage(pageToShow);
                    currentPage = pageToShow;
                    shouldForceUpdate = true;
                }),
            ButtonStyle::Pill,
            "settings",
            resources.dejavuSansFont,
            secondary_text_color,
            "settings"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        // Mixer
        button(
            Modifier()
                .align(Align::RIGHT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color)
                .onLClick([&](){
                    showMixer = !showMixer;
                    const std::string pageToShow = showMixer ? "mixer" : "timeline";
                    ui->switchToPage(pageToShow);
                    currentPage = pageToShow;
                    shouldForceUpdate = true;
                }),
            ButtonStyle::Pill,
            "mixer",
            resources.dejavuSansFont,
            secondary_text_color,
            "mixer"
        ),
        
        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
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
                .align(Align::CENTER_X)
                .onLClick([&](){
                    std::string selectedDir = selectDirectory();
                    if (!selectedDir.empty() && std::filesystem::is_directory(selectedDir)) {
                        fileTree.setRootDirectory(selectedDir);
                        buildFileTreeUI();
                    }
                }),
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
            // Prevent rapid multiple clicks
            static std::string lastFilePath = "";
            static auto lastClickTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime);
            
            // Only process if it's a different file or enough time has passed
            if (filePath != lastFilePath || timeDiff.count() > 500) {
                std::cout << "Loading sample from file browser: " << filePath << std::endl;
                newTrack(filePath);
                lastFilePath = filePath;
                lastClickTime = now;
            } else {
                std::cout << "Ignoring rapid click on same file: " << filePath << std::endl;
            }
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
                    SliderOrientation::Vertical,
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
            Modifier().setfixedWidth(32).setHeight(1.f).align(Align::CENTER_X | Align::BOTTOM),
            slider_knob_color,
            slider_bar_color,
            SliderOrientation::Vertical,
            trackName + "_mixer_volume_slider"
        ),

        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        contains{
            slider(
                Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
                slider_knob_color,
                slider_bar_color,
                SliderOrientation::Horizontal,
                trackName + "_mixer_pan_slider"
            ),
        }),

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
            SliderOrientation::Vertical,
            "Master_mixer_volume_slider"
        ),
        spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),

        row(Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
        contains{
            slider(
                Modifier().setWidth(.8f).setfixedHeight(32.f).align(Align::BOTTOM | Align::CENTER_X),
                slider_knob_color,
                slider_bar_color,
                SliderOrientation::Horizontal,
                "Master_mixer_pan_slider"
            ),
        }),

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
                    SliderOrientation::Vertical,
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
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), selectedThemeName, resources.dejavuSansFont, "theme_text"),
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
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), currentSampleRate, resources.dejavuSansFont, "sample_rate_text"),
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
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible() && !textInputActive && !bpmInputActive && !autosaveInputActive) {
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
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible() && !textInputActive && !projectNameInputActive && !autosaveInputActive) {
                    bpmInputActive = true;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), bpmValue, resources.dejavuSansFont, "bpm_box"),
            }, "bpm_row"),
            spacer(Modifier().setfixedWidth(32.f).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(16.f)),
        // Autosave interval input
        row(Modifier().setfixedHeight(32), contains{
            spacer(Modifier().setfixedWidth(32.f)),
            text(Modifier().setfixedHeight(32).setColor(primary_text_color).align(Align::LEFT | Align::CENTER_Y), "Auto-save Interval (sec)", resources.dejavuSansFont, "autosave_label"),
            row(Modifier().setfixedHeight(32).setfixedWidth(256).align(Align::RIGHT).setColor(sf::Color::White).onLClick([&](){
                if (!dropdownMenu->m_modifier.isVisible() && !sampleRateDropdownMenu->m_modifier.isVisible() && !textInputActive && !projectNameInputActive && !bpmInputActive) {
                    autosaveInputActive = true;
                    std::cout << "Autosave input activated" << std::endl;
                }
            }), contains{
                spacer(Modifier().setfixedWidth(8.f)),
                text(Modifier().setfixedHeight(28).setColor(sf::Color::Black).align(Align::LEFT | Align::CENTER_Y), autosaveValue, resources.dejavuSansFont, "autosave_box"),
            }, "autosave_row"),
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
                        
                        std::cout << "Selected: " << item << " (will apply on next startup)" << std::endl;
                        
                        // Update the theme text in the settings
                        if (texts.count("theme_text")) {
                            texts["theme_text"]->setString(item);
                        }
                        
                        // Update current theme and save to config (theme will apply on next startup)
                        selectedThemeName = item;
                        saveConfig();
                        
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
                        
                        // Update current sample rate and save to config
                        currentSampleRate = item;
                        saveConfig();
                        
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

    // Prevent rapid duplicate track creation
    static std::string lastSamplePath = "";
    static auto lastTrackCreationTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTrackCreationTime);
    
    if (samplePath == lastSamplePath && timeDiff.count() < 1000) {
        std::cout << "Ignoring rapid duplicate track creation for: " << samplePath << std::endl;
        return;
    }
    
    lastSamplePath = samplePath;
    lastTrackCreationTime = now;

    std::string trackName;
    int trackIndex = uiState.trackCount;

    if (!samplePath.empty()) {
        juce::File sampleFile(samplePath);
        trackName = sampleFile.getFileNameWithoutExtension().toStdString();
        
        // Check if track with this name already exists
        for (const auto& track : engine.getAllTracks()) {
            if (track->getName() == trackName) {
                std::cout << "Track with name '" << trackName << "' already exists, skipping creation" << std::endl;
                return;
            }
        }
        
        engine.addTrack(trackName, samplePath);
        
        // Set default pan to center
        engine.getTrack(trackIndex)->setPan(0.5f);

        std::cout << "Loaded sample: " << samplePath << " into Track '" << trackName << "' (" << (trackIndex + 1) << ")" << std::endl;
    } 
    else {
        trackName = "Track_" + std::to_string(trackIndex + 1);
        engine.addTrack(trackName);
        
        // Set default pan to center
        engine.getTrack(trackIndex)->setPan(0.5f);
        
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
    
    // Force UI update to ensure elements are available
    ui->forceUpdate();
    
    // Set pan slider to center position for new track
    if (getSlider(trackName + "_mixer_pan_slider")) {
        getSlider(trackName + "_mixer_pan_slider")->setValue(0.5f);
        std::cout << "Set pan slider for " << trackName << " to 0.5f" << std::endl;
    } else {
        std::cout << "Pan slider for " << trackName << " not found!" << std::endl;
    }
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
            
            // Convert engine pan (-1 to 1) to slider value (0 to 1)
            float panSliderValue = (t->getPan() + 1.0f) / 2.0f;
            getSlider(t->getName() + "_mixer_pan_slider")->setValue(panSliderValue);
            std::cout << "Set " << t->getName() << " pan slider to " << panSliderValue << " (engine pan: " << t->getPan() << ")" << std::endl;
        }
    }

    undoStack.push(engine.getStateString());
    
    // Update project settings values
    projectNameValue = engine.getCurrentCompositionName();
    bpmValue = std::to_string(static_cast<int>(engine.getBpm()));
}

bool Application::handleTrackEvents() {
    bool shouldForceUpdate = false;

    if (getButton("mute_Master") && getButton("mute_Master")->isClicked()) {
        engine.getMasterTrack()->toggleMute();
        getButton("mute_Master")->m_modifier.setColor((engine.getMasterTrack()->isMuted() ? mute_color : not_muted_color));
        std::cout << "Master track mute state toggled to " << ((engine.getMasterTrack()->isMuted()) ? "true" : "false") << std::endl;

        shouldForceUpdate = true;
    }

    // Handle master track solo button
    if (getButton("solo_Master") && getButton("solo_Master")->isClicked()) {
        bool wasSolo = engine.getMasterTrack()->isSolo();
        
        if (wasSolo) {
            // Un-solo master track
            engine.getMasterTrack()->setSolo(false);
        } else {
            // Solo master track and un-solo all other tracks
            engine.getMasterTrack()->setSolo(true);
            for (auto& track : engine.getAllTracks()) {
                track->setSolo(false);
            }
        }
        
        // Update button colors
        if (getButton("solo_Master")) {
            getButton("solo_Master")->m_modifier.setColor(
                (engine.getMasterTrack()->isSolo() ? mute_color : button_color)
            );
        }
        for (auto& track : engine.getAllTracks()) {
            if (getButton("solo_" + track->getName())) {
                getButton("solo_" + track->getName())->m_modifier.setColor(
                    (track->isSolo() ? mute_color : button_color)
                );
            }
        }
        
        std::cout << "Master track solo state toggled to " << ((engine.getMasterTrack()->isSolo()) ? "true" : "false") << std::endl;
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

    // Handle Master pan sliders (mixer only)
    float masterSliderValue = (engine.getMasterTrack()->getPan() + 1.0f) / 2.0f;
    if (getSlider("Master_mixer_pan_slider")->getValue() != masterSliderValue) {
        float sliderPan = getSlider("Master_mixer_pan_slider")->getValue();
        float newPan = (sliderPan * 2.0f) - 1.0f; // Convert slider (0-1) to engine (-1 to 1)
        engine.getMasterTrack()->setPan(newPan);
        std::cout << "Master track pan changed to: " << newPan << " (slider: " << sliderPan << ")" << std::endl;

        shouldForceUpdate = true;
    }

    for (auto& track : engine.getAllTracks()) {
        if (getButton("mute_" + track->getName())->isClicked()) {
            track->toggleMute();
            getButton("mute_" + track->getName())->m_modifier.setColor((track->isMuted() ? mute_color : not_muted_color));
            std::cout << "Track '" << track->getName() << "' mute state toggled to " << ((track->isMuted()) ? "true" : "false") << std::endl;

            shouldForceUpdate = true;
        }

        // Handle solo button clicks
        if (getButton("solo_" + track->getName()) && getButton("solo_" + track->getName())->isClicked()) {
            bool wasSolo = track->isSolo();
            
            // If this track was the only one soloed, un-solo it
            if (wasSolo) {
                // Check if this is the only soloed track
                bool isOnlySoloedTrack = true;
                for (auto& otherTrack : engine.getAllTracks()) {
                    if (otherTrack != track && otherTrack->isSolo()) {
                        isOnlySoloedTrack = false;
                        break;
                    }
                }
                
                if (isOnlySoloedTrack) {
                    // Un-solo this track
                    track->setSolo(false);
                } else {
                    // There are other soloed tracks, so just solo this one (un-solo others)
                    for (auto& otherTrack : engine.getAllTracks()) {
                        otherTrack->setSolo(otherTrack == track);
                    }
                }
            } else {
                // Solo this track and un-solo all others
                for (auto& otherTrack : engine.getAllTracks()) {
                    otherTrack->setSolo(otherTrack == track);
                }
            }
            
            // Update button colors for all tracks
            for (auto& updateTrack : engine.getAllTracks()) {
                if (getButton("solo_" + updateTrack->getName())) {
                    getButton("solo_" + updateTrack->getName())->m_modifier.setColor(
                        (updateTrack->isSolo() ? mute_color : button_color)
                    );
                }
            }
            
            std::cout << "Track '" << track->getName() << "' solo state toggled to " << ((track->isSolo()) ? "true" : "false") << std::endl;
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

        // Handle track pan sliders (mixer only)
        float trackSliderValue = (track->getPan() + 1.0f) / 2.0f;
        if (getSlider(track->getName() + "_mixer_pan_slider")->getValue() != trackSliderValue) {
            float sliderPan = getSlider(track->getName() + "_mixer_pan_slider")->getValue();
            float newPan = (sliderPan * 2.0f) - 1.0f; // Convert slider (0-1) to engine (-1 to 1)
            track->setPan(newPan);
            std::cout << "Track '" << track->getName() << "' pan changed to: " << newPan << " (slider: " << sliderPan << ")" << std::endl;

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
        
        // Convert engine pan (-1 to 1) to slider value (0 to 1)
        float panSliderValue = (t->getPan() + 1.0f) / 2.0f;
        getSlider(t->getName() + "_mixer_pan_slider")->setValue(panSliderValue);
    }
    
    // Set master mixer pan slider to center position (convert engine pan to slider value)
    if (getSlider("Master_mixer_pan_slider")) {
        float masterPanSliderValue = (engine.getMasterTrack()->getPan() + 1.0f) / 2.0f;
        getSlider("Master_mixer_pan_slider")->setValue(masterPanSliderValue);
    }
}

void Application::rebuildUI() {
    // --- Destroy the entire UI and all owned elements/maps ---
    if (ui) {
        delete ui;
        ui = nullptr;
    }
    // DO NOT use or clear any previous element pointers after this point!

    // --- Rebuild persistent UI elements (fresh pointers) ---
    topRowElement             = topRow();
    fileBrowserElement        = fileBrowser();
    masterTrackElement        = masterTrack();
    timelineElement           = timeline();
    mixerElement              = mixer();
    masterMixerTrackElement   = masterMixerTrack();
    browserAndTimelineElement = browserAndTimeline();
    browserAndMixerElement    = browserAndMixer();
    fxRackElement             = fxRack();

    // if (timelineElement) timelineElement->setScrollSpeed(20.f);
    if (mixerElement)    mixerElement->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(not_muted_color),
    contains {
        button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "options" << std::endl;}), 
            ButtonStyle::Rect, "Options", resources.dejavuSansFont, black, "cm_options"),
        spacer(Modifier().setfixedHeight(1)),

        button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "rename" << std::endl;}), 
            ButtonStyle::Rect, "Rename", resources.dejavuSansFont, black, "cm_rename"),
        spacer(Modifier().setfixedHeight(1)),

        button(Modifier().setfixedHeight(32).setColor(white).onLClick([&](){std::cout << "change color" << std::endl;}), 
            ButtonStyle::Rect, "Change Color", resources.dejavuSansFont, black, "cm_change_color"),
        spacer(Modifier().setfixedHeight(1)),
    });
    contextMenu->hide();

    // --- Recreate UILO and all pages ---
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
        }), "timeline"
    }});

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
        }), "mixer"
    });

    ui->addPage({
        page({
            column(
                Modifier()
                    .setfixedWidth(400)
                    .setfixedHeight(120)
                    .align(Align::CENTER_X | Align::CENTER_Y),
            contains{
                text(
                    Modifier()
                        .setfixedWidth(300)
                        .setfixedHeight(24)
                        .align(Align::CENTER_X | Align::CENTER_Y),
                    "Auto-save interval (sec):",
                    resources.dejavuSansFont
                ),
                slider(
                    Modifier().setfixedWidth(15).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
                    sf::Color::White,
                    sf::Color::Black,
                    SliderOrientation::Vertical,
                    "Auto-save_interval_slider"
                ),
                spacer(Modifier().setfixedHeight(12).align(Align::BOTTOM)),
            })
        }), "settings"
    });

    // --- Repopulate timeline/mixer tracks from engine ---
    for (auto& t : engine.getAllTracks()) {
        if (!t) continue;
        const std::string& name = t->getName();
        if (name == "Master") continue;

        // Timeline
        if (timelineElement)
            timelineElement->addElements({
                spacer(Modifier().setfixedHeight(2).align(Align::TOP)),
                track(name, Align::TOP | Align::LEFT),
            });

        // Mixer
        if (mixerElement)
            mixerElement->addElements({
                spacer(Modifier().setfixedWidth(2).align(Align::LEFT)),
                mixerTrack(name, Align::TOP | Align::LEFT, decibelsToFloat(t->getVolume()), t->getPan()),
            });

        // Restore sliders (safe-guarded)
        auto volSlider = getSlider(name + "_volume_slider");
        if (volSlider)
            volSlider->setValue(decibelsToFloat(t->getVolume()));
        auto mixSlider = getSlider(name + "_mixer_volume_slider");
        if (mixSlider)
            mixSlider->setValue(decibelsToFloat(t->getVolume()));
    }

    // --- Master track sliders ---
    if (engine.getMasterTrack()) {
        auto* master = engine.getMasterTrack();
        auto masterVol = getSlider("Master_volume_slider");
        if (masterVol)
            masterVol->setValue(decibelsToFloat(master->getVolume()));
        auto masterMix = getSlider("Master_mixer_volume_slider");
        if (masterMix)
            masterMix->setValue(decibelsToFloat(master->getVolume()));
    }

    // --- Restore settings slider ---
    auto autosaveSlider = getSlider("autosave_interval_slider");
    if (autosaveSlider)
        autosaveSlider->setValue(static_cast<float>(uiState.autoSaveIntervalSeconds));

    // --- Show correct page (default to timeline if not set) ---
    running = ui->isRunning();
    if (currentPage.empty() || !ui) currentPage = "timeline";
    ui->switchToPage(currentPage);
    ui->forceUpdate();
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

void Application::loadConfig() {
    std::ifstream in(configFilePath);
    if (!in) return;
    try {
        nlohmann::json j;
        in >> j;
        if (j.contains("autoSaveIntervalSeconds") && j["autoSaveIntervalSeconds"].is_number_integer()) {
            autoSaveIntervalSeconds = j["autoSaveIntervalSeconds"];
            uiState.autoSaveIntervalSeconds = autoSaveIntervalSeconds;
            std::cout << "Loaded auto-save interval: " << autoSaveIntervalSeconds << " seconds" << std::endl;
        }
        if (j.contains("currentTheme") && j["currentTheme"].is_string()) {
            selectedThemeName = j["currentTheme"];
            std::cout << "Loaded theme: " << selectedThemeName << std::endl;
        }
        if (j.contains("currentSampleRate") && j["currentSampleRate"].is_string()) {
            currentSampleRate = j["currentSampleRate"];
            std::cout << "Loaded sample rate: " << currentSampleRate << " Hz" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config: " << e.what() << "\n";
    }
}

void Application::saveConfig() {
    nlohmann::json j;
    j["autoSaveIntervalSeconds"] = autoSaveIntervalSeconds;
    j["currentTheme"] = selectedThemeName;
    j["currentSampleRate"] = currentSampleRate;
    std::ofstream out(configFilePath);
    if (!out) {
        std::cerr << "Cannot write config\n"; return;
    }
    out << j.dump(4);
    std::cout << "Config saved: theme=" << selectedThemeName << ", sampleRate=" << currentSampleRate << ", autoSave=" << autoSaveIntervalSeconds << "s" << std::endl;
}

void Application::checkAutoSave() {
    if (autoSaveTimer.getElapsedTime().asSeconds() >= autoSaveIntervalSeconds && !uiState.saveDirectory.empty()) {
        std::string autosaveFilename = engine.getCurrentCompositionName() + "_autosave.mpf";
        std::string autosavePath = uiState.saveDirectory + "/" + autosaveFilename;
        
        std::cout << "Attempting auto-save to: " << autosavePath << std::endl;
        std::string currentState = engine.getStateString();
        
        if (engine.saveState(autosavePath)) {
            std::cout << "Auto-saved to " << autosavePath << std::endl;
            
            if (std::filesystem::exists(autosavePath)) {
                auto fileSize = std::filesystem::file_size(autosavePath);
                std::cout << "Auto-save file size: " << fileSize << " bytes" << std::endl;
                std::cout << "Saved composition: " << engine.getCurrentCompositionName() << std::endl;
                std::cout << "Number of tracks: " << engine.getAllTracks().size() << std::endl;
            } else {
                std::cerr << "Auto-save file was not created!" << std::endl;
            }
        } else {
            std::cerr << "Auto-save failed to " << autosavePath << std::endl;
        }
        autoSaveTimer.restart();
    }
}

void Application::applyThemeByName(const std::string& themeName) {
    if (themeName == "Default") {
        applyTheme(Themes::Default);
    } else if (themeName == "Dark") {
        applyTheme(Themes::Dark);
    } else if (themeName == "Light") {
        applyTheme(Themes::Light);
    } else if (themeName == "Cyberpunk") {
        applyTheme(Themes::Cyberpunk);
    } else if (themeName == "Forest") {
        applyTheme(Themes::Forest);
    } else {
        // Default fallback
        applyTheme(Themes::Default);
        std::cout << "Unknown theme '" << themeName << "', using Default theme" << std::endl;
    }
    std::cout << "Applied theme: " << themeName << std::endl;
}