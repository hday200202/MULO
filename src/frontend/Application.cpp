#include "Application.hpp"

Application::Application() {
    engine.newComposition("untitled");
    engine.addTrack("Master");
    initUIResources();

    // Persistent UI Elements
    topRowElement               = topRow();
    fileBrowserElement          = fileBrowser();
    masterTrackElement          = masterTrack();
    timelineElement             = timeline();
    mixerElement                = mixer();
    masterMixerTrackElement     = mixerTrack("Master");
    browserAndTimelineElement   = browserAndTimeline();
    browserAndMixerElement      = browserAndMixer();
    fxRackElement               = fxRack();

    // Base UI
    ui = new UILO("MULO", {{
        page({
            column(
                Modifier(),
            contains{
                topRowElement,
                browserAndTimelineElement,
                fxRackElement,
            }),
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
        }), "mixer" }
        
    );

    if (ui) running = ui->isRunning();
    ui->switchToPage("timeline");

    loadComposition("assets/empty_project.mpf");
}

Application::~Application() {
    delete ui;
    ui = nullptr;
}

void Application::update() {
    running = ui->isRunning();

    if (ui->isRunning() && running) {
        if (buttons["select_directory"]->isClicked()) {
            std::string dir = selectDirectory();
            if (!dir.empty()) {
                uiState.fileBrowserDirectory = dir;
                std::cout << "Selected directory: " << dir << std::endl;
            }
        }

        if (buttons["mixer"]->isClicked()) {
            showMixer = !showMixer;

            if (showMixer)
                ui->switchToPage("mixer");
            else
                ui->switchToPage("timeline");
        }

        if (buttons["new_track"]->isClicked()) {
            newTrack();
            std::cout << "New track added. Total tracks: " << uiState.trackCount << std::endl;
        }

        if (buttons["save"]->isClicked()) {
            if (engine.saveState(selectDirectory() + "/" + engine.getCurrentCompositionName() + ".mpf"))
                std::cout << "Project saved successfully." << std::endl;
            else
                std::cerr << "Failed to save project." << std::endl;
        }

        if (buttons["load"]->isClicked()) {
            std::string path = selectFile({"*.mpf"});
            if (!path.empty())
                loadComposition(path);
            else
                std::cout << "No file selected." << std::endl;
        }

        if (buttons["play"]->isClicked() && !playing) {
            std::cout << "Playing audio..." << std::endl;
            engine.play();
            buttons["play"]->setText("pause");
            playing = true;
        }

        else if (buttons["play"]->isClicked() && playing) {
            std::cout << "Pausing audio..." << std::endl;
            engine.pause();
            engine.setPosition(0.0);
            buttons["play"]->setText("play");
            playing = false;
        }

        handleTrackEvents();

        ui->update();
    }
}

void Application::render() {
    ui->render();
}

bool Application::isRunning() const {
    return running;
}

void Application::initUIResources() {
    juce::File fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/DejaVuSans.ttf");
    if (!fontFile.existsAsFile()) {
        // Try relative to executable
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
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
            ButtonStyle::Pill, 
            "load", 
            resources.openSansFont, 
            sf::Color(230, 230, 230),
            "load"
        ),

        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
            ButtonStyle::Pill,
            "save",
            resources.openSansFont,
            sf::Color::White,
            "save"
        ),

        button(
            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
            ButtonStyle::Pill,
            "play",
            resources.openSansFont,
            sf::Color::White,
            "play"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
            ButtonStyle::Pill,
            "mixer",
            resources.openSansFont,
            sf::Color::White,
            "mixer"
        ),

        spacer(Modifier().setfixedWidth(12).align(Align::RIGHT)),

        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
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
            timelineElement
        })
    });
}

Row* Application::browserAndMixer() {
    return row(
        Modifier().setWidth(1.f).setHeight(1.f),
    contains{
        fileBrowserElement,
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
            sf::Color(230, 230, 230),
            "select_directory"
        ),
    });
}

Column* Application::timeline() {
    return column(
        Modifier(),
    contains{
        // track("Master", Align::LEFT | Align::BOTTOM)
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

            // spacer(Modifier().setfixedWidth(16).align(Align::CENTER_Y)),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        })
    });
}

Column* Application::mixerTrack(const std::string& trackName, Align alignment, float volume, float pan) {
    return column(
        Modifier()
            .setColor(sf::Color(120, 120, 120))
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
            }),

            // spacer(Modifier().setfixedWidth(16).align(Align::CENTER_Y)),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        })
    });
}

Row* Application::mixer() {
    return row(
        Modifier().setWidth(1.f).setHeight(1.f).setColor(sf::Color(100, 100, 100)),
    contains{
       
    }, "mixer");
}

std::string Application::selectDirectory() {
    juce::FileChooser chooser("Select directory", juce::File(), "*");
    if (chooser.browseForDirectory()) {
        return chooser.getResult().getFullPathName().toStdString();
    }
    return "";
}

std::string Application::selectFile(std::initializer_list<std::string> filters) {
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

    timelineElement->addElement(
        masterTrackElement
    );

    mixerElement->addElement(
        masterMixerTrackElement
    );

    uiState.trackCount = engine.getAllTracks().size();
    
    for (auto& t : engine.getAllTracks()) {
        std::cout << "Loaded track: " << t->getName() << std::endl;

        if (t->getName() == "Master") {
            uiState.masterTrack.pan = t->getPan();
            uiState.masterTrack.volume = t->getVolume();
            uiState.masterTrack.name = t->getName();
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

            sliders[t->getName() + "_volume_slider"]->setValue(decibelsToFloat(t->getVolume()));
            sliders[t->getName() + "_mixer_volume_slider"]->setValue(decibelsToFloat(t->getVolume()));
        }
    }
}

void Application::handleTrackEvents() {
    for (auto& track : engine.getAllTracks()) {
        if (buttons["mute_" + track->getName()]->isClicked()) {
            track->toggleMute();
            buttons["mute_" + track->getName()]->m_modifier.setColor((track->isMuted() ? sf::Color::Red : sf::Color(50, 50, 50)));
            std::cout << "Track '" << track->getName() << "' mute state toggled to " << ((track->isMuted()) ? "true" : "false") << std::endl;
        }

        if (floatToDecibels(sliders[track->getName() + "_volume_slider"]->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(sliders[track->getName() + "_volume_slider"]->getValue());
            track->setVolume(newVolume);
            sliders[track->getName() + "_mixer_volume_slider"]->setValue(sliders[track->getName() + "_volume_slider"]->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;
        }

        if (floatToDecibels(sliders[track->getName() + "_mixer_volume_slider"]->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(sliders[track->getName() + "_mixer_volume_slider"]->getValue());
            track->setVolume(newVolume);
            sliders[track->getName() + "_volume_slider"]->setValue(sliders[track->getName() + "_mixer_volume_slider"]->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;
        }
    }
}

void Application::undo() {
    
}

void Application::redo() {
    
}