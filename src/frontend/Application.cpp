#include <nlohmann/json.hpp>
#include "Application.hpp"
#include <fstream>

Application::Application() {
    // Initialize auto-save timer from config
    loadConfig();
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

    engine.newComposition("untitled");
    engine.addTrack("Master");
    initUIResources();

    // 3) Build persistent UI elements
    topRowElement             = topRow();
    fileBrowserElement        = fileBrowser();
    masterTrackElement        = masterTrack();
    timelineElement           = timeline();
    mixerElement              = mixer();
    masterMixerTrackElement   = masterMixerTrack();
    browserAndTimelineElement = browserAndTimeline();
    browserAndMixerElement    = browserAndMixer();
    fxRackElement             = fxRack();
    timelineElement->setScrollSpeed(20.f);
    mixerElement   ->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(sf::Color(50,50,50)),
        contains {
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout<<"options\n"; }),
                   ButtonStyle::Rect, "Options", resources.openSansFont, sf::Color::Black, "cm_options"),
            spacer(Modifier().setfixedHeight(1)),
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout<<"rename\n"; }),
                   ButtonStyle::Rect, "Rename", resources.openSansFont, sf::Color::Black, "cm_rename"),
            spacer(Modifier().setfixedHeight(1)),
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout<<"change color\n"; }),
                   ButtonStyle::Rect, "Change Color", resources.openSansFont, sf::Color::Black, "cm_change_color"),
            spacer(Modifier().setfixedHeight(1)),
        }
    );
    contextMenu->hide();

    // 4) Build pages
    ui = new UILO(window, windowView, {{
        page({ column(Modifier(), contains{ topRowElement, browserAndTimelineElement, fxRackElement }), contextMenu }), "timeline"
    }});
    ui->addPage({
        page({ column(Modifier(), contains{ topRowElement, browserAndMixerElement, fxRackElement }), contextMenu }), "mixer"
    });
    ui->addPage({
        page({
            column(
                Modifier().setfixedWidth(400).setfixedHeight(120).align(Align::CENTER_X|Align::CENTER_Y),
                contains{
                    text(Modifier().setfixedWidth(300).setfixedHeight(24).align(Align::CENTER_X|Align::CENTER_Y),
                         "Auto-save interval (sec):", resources.openSansFont),
                    slider(Modifier().setfixedWidth(300), sf::Color::White, sf::Color::Black, "autosave_interval_slider")
                }
            )
        }), "settings"
    });

    // 5) Initialize slider to config
    getSlider("autosave_interval_slider")->setValue((float)uiState.autoSaveIntervalSeconds);

    // 6) Finalize
    running = ui->isRunning();
    ui->switchToPage("timeline");
    loadComposition("assets/empty_project.mpf");
    ui->forceUpdate();
}


Application::~Application(){
    delete ui;
    ui = nullptr;
}

void Application::update() {
    running = ui->isRunning();
    if (!running) return;

    // Auto-save
    checkAutoSave();

    // Settings page slider
    if (currentPage == "settings") {
        int newInterval = (int)getSlider("autosave_interval_slider")->getValue();
        if (newInterval != uiState.autoSaveIntervalSeconds) {
            uiState.autoSaveIntervalSeconds = newInterval;
            autoSaveIntervalSeconds         = newInterval;
            saveConfig();
            ui->forceUpdate();
        }
    }

    // UI events
    bool dirty = false;
    dirty |= handleContextMenu();
    dirty |= handleUIButtons();
    dirty |= handlePlaybackControls();
    dirty |= handleTrackEvents();
    dirty |= handleKeyboardShortcuts();

    if (dirty) ui->forceUpdate();
    else       ui->update(windowView);

    // Deferred loads
    if (!pendingLoadPath.empty()) {
        loadComposition(pendingLoadPath);
        pendingLoadPath.clear();
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

    // Helper lambda for safe button lookup and click detection
    auto isClicked = [](const std::string& key) -> bool {
        auto it = buttons.find(key);
        return (it != buttons.end() && it->second && it->second->isClicked());
    };

    if (isClicked("select_directory")) {
        std::string dir = selectDirectory();
        if (!dir.empty()) {
            uiState.fileBrowserDirectory = dir;
            std::cout << "Selected directory: " << dir << "\n";
        }
        shouldForceUpdate = true;
    }

    if (isClicked("mixer")) {
        showMixer = !showMixer;
        const std::string pageToShow = showMixer ? "mixer" : "timeline";
        ui->switchToPage(pageToShow);
        currentPage = pageToShow;
        shouldForceUpdate = true;
    }

    if (isClicked("new_track")) {
        uiChanged = true;
        newTrack();
        std::cout << "New track added. Total tracks: " << uiState.trackCount << "\n";
        shouldForceUpdate = true;
    }

    if (isClicked("save")) {
        std::string path = selectDirectory() + "/" + engine.getCurrentCompositionName() + ".mpf";
        bool ok = engine.saveState(path);
        std::cout << (ok ? "Project saved.\n" : "Save failed.\n");
        shouldForceUpdate = true;
    }

    if (isClicked("load")) {
        std::string path = selectFile({"*.mpf"});
        if (!path.empty())
            loadComposition(path);
        shouldForceUpdate = true;
    }

    if (isClicked("settings")) {
        ui->switchToPage("settings");
        currentPage = "settings";
        shouldForceUpdate = true;
    }

    return shouldForceUpdate;
}

bool Application::handlePlaybackControls() {
    static bool prevSpace = false;
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool shouldForceUpdate = false;

    // Safe lookup for play button
    Button* playBtn = nullptr;
    auto it = buttons.find("play");
    if (it != buttons.end())
        playBtn = it->second;

    bool playClicked = (playBtn && playBtn->isClicked());
    bool spacePressed = (space && !prevSpace);

    if ((playClicked || spacePressed) && !playing) {
        std::cout << "Playing audio..." << std::endl;
        engine.play();
        if (playBtn) playBtn->setText("pause");
        playing = true;
        shouldForceUpdate = true;
    } else if ((playClicked || spacePressed) && playing) {
        std::cout << "Pausing audio..." << std::endl;
        engine.pause();
        engine.setPosition(0.0);
        if (playBtn) playBtn->setText("play");
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

        // Load
        button(
            Modifier()
                .align(Align::LEFT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color),
            ButtonStyle::Pill,
            "load",
            resources.openSansFont,
            sf::Color(230,230,230),
            "load"
        ),
        spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

        // Save
        button(
            Modifier()
                .align(Align::LEFT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color),
            ButtonStyle::Pill,
            "save",
            resources.openSansFont,
            sf::Color::White,
            "save"
        ),
        spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

        // Play/Pause
        button(
            Modifier()
                .align(Align::CENTER_X | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color),
            ButtonStyle::Pill,
            playing ? "pause" : "play",
            resources.openSansFont,
            sf::Color::White,
            "play"
        ),
        spacer(Modifier().setfixedWidth(24).align(Align::LEFT)),

        // Auto-save label
        text(
            Modifier()
                .setfixedWidth(120)
                .setfixedHeight(24)
                .align(Align::CENTER_Y),
            "Auto-save (sec):",
            resources.openSansFont
        ),
        // Auto-save slider
        slider(
            Modifier()
                .setfixedWidth(120)
                .align(Align::CENTER_Y),
            sf::Color::White,
            sf::Color::Black,
            "autosave_interval_slider"
        ),
        // Display current interval
        text(
            Modifier()
                .setfixedWidth(40)
                .setfixedHeight(24)
                .align(Align::CENTER_Y),
            std::to_string(uiState.autoSaveIntervalSeconds),
            resources.openSansFont
        ),
        spacer(Modifier().setfixedWidth(24).align(Align::LEFT)),

        // Mixer toggle
        button(
            Modifier()
                .align(Align::RIGHT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color),
            ButtonStyle::Pill,
            showMixer ? "timeline" : "mixer",
            resources.openSansFont,
            sf::Color::White,
            "mixer"
        ),
        spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

        // + Track
        button(
            Modifier()
                .align(Align::RIGHT | Align::CENTER_Y)
                .setfixedWidth(96).setHeight(.75f)
                .setColor(button_color),
            ButtonStyle::Pill,
            "+ track",
            resources.openSansFont,
            sf::Color::White,
            "new_track"
        ),
        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
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
        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(track_color),
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
    });
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

        engine.getTrack(trackIndex)->addClip(AudioClip(sampleFile, 0.0, 0.0, lengthSeconds, 1.0f));
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

    if (getButton("mute_Master") && getButton("mute_Master")->isClicked()) {
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
void Application::rebuildUI()
{
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

    if (timelineElement) timelineElement->setScrollSpeed(20.f);
    if (mixerElement)    mixerElement->setScrollSpeed(20.f);

    contextMenu = freeColumn(
        Modifier().setfixedHeight(400).setfixedWidth(200).setColor(sf::Color(50, 50, 50)),
        contains {
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout << "options\n"; }),
                   ButtonStyle::Rect, "Options", resources.openSansFont, sf::Color::Black, "cm_options"),
            spacer(Modifier().setfixedHeight(1)),
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout << "rename\n"; }),
                   ButtonStyle::Rect, "Rename", resources.openSansFont, sf::Color::Black, "cm_rename"),
            spacer(Modifier().setfixedHeight(1)),
            button(Modifier().setfixedHeight(32).setColor(sf::Color::White).onClick([&](){ std::cout << "change color\n"; }),
                   ButtonStyle::Rect, "Change Color", resources.openSansFont, sf::Color::Black, "cm_change_color"),
            spacer(Modifier().setfixedHeight(1)),
        }
    );
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
                    resources.openSansFont
                ),
                slider(
                    Modifier().setfixedWidth(15).setHeight(1.f).align(Align::BOTTOM | Align::CENTER_X),
                    sf::Color::White,
                    sf::Color::Black,
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
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config: " << e.what() << "\n";
    }
}

void Application::saveConfig() {
    nlohmann::json j;
    j["autoSaveIntervalSeconds"] = autoSaveIntervalSeconds;
    std::ofstream out(configFilePath);
    if (!out) {
        std::cerr << "Cannot write config\n"; return;
    }
    out << j.dump(4);
}

void Application::checkAutoSave() {
    if (autoSaveTimer.getElapsedTime().asSeconds() >= autoSaveIntervalSeconds) {
        std::string path = engine.getCurrentCompositionName() + "_autosave.mpf";
        if (engine.saveState(path))
            std::cout << "Auto-saved to " << path << "\n";
        else
            std::cerr << "Auto-save failed\n";
        autoSaveTimer.restart();
    }
}
