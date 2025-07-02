#pragma once

#include "UILO.hpp"
#include "Engine.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

using namespace uilo;

void application() {
    Engine engine;
    engine.newComposition("test");

    // Add tracks
    engine.addTrack("Track 1");
    engine.addTrack("Track 2");

    auto* kick = engine.getTrack(0);
    auto* snare = engine.getTrack(1);

    // Load sample clips into tracks
    juce::File kickSample("assets/test_samples/kick.wav");
    juce::File snareSample("assets/test_samples/snare.wav");

    AudioClip kickClip(
        kickSample,                     // Sample file
        0.0,                            // Start Time  (relative to whole composition)
        0.0,                            // Start Time  (relative to sample)
        kickSample.getSize() / 44100.0  // Sample Rate (don't worry about this)
    );

    AudioClip snareClip1(
        snareSample, 
        25.0, 
        0.0, 
        snareSample.getSize() / 44100.0
    );

    kick->addClip(kickClip);
    snare->addClip(snareClip1);

    UILO ui("MULO", {{
        page({
            column(
                Modifier(),
            contains{
                row(
                    Modifier()
                        .setWidth(1.f)
                        .setfixedHeight(64)
                        .setColor(sf::Color(200, 200, 200)),
                contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                    button(
                        Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
                        ButtonStyle::Pill, 
                        "Load", 
                        "assets/fonts/OpenSans-Regular.ttf", 
                        sf::Color(230, 230, 230),
                        "LOAD"
                    ),

                    spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),

                    button(
                        Modifier().align(Align::RIGHT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
                        ButtonStyle::Pill,
                        "Save",
                        "assets/fonts/OpenSans-Regular.ttf",
                        sf::Color::White,
                        "SAVE"
                    ),

                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
                }),

                row(
                    Modifier().setWidth(1.f).setHeight(1.f),
                contains{
                    column(
                        Modifier()
                            .align(Align::LEFT)
                            .setfixedWidth(256)
                            .setColor(sf::Color(155, 155, 155)),
                    contains{
                        
                    }),

                    row(
                        Modifier()
                            .setWidth(1.f)
                            .setHeight(1.f)
                            .setColor(sf::Color(100, 100, 100)),
                    contains{
                        column(
                            Modifier(),
                        contains{
                            row(
                                Modifier()
                                    .setColor(sf::Color(120, 120, 120))
                                    .setfixedHeight(96)
                                    .align(Align::BOTTOM),
                            contains{
                                row(
                                    Modifier()
                                        .align(Align::RIGHT)
                                        .setfixedWidth(150)
                                        .setColor(sf::Color(155, 155, 155)),
                                contains{
                                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                                    text(
                                        Modifier().setColor(sf::Color(25, 25, 25)).setfixedWidth(0.5).setHeight(0.25).align(Align::CENTER_Y),
                                        "Master",
                                        "assets/fonts/OpenSans-Regular.ttf"
                                    ),

                                    slider(
                                        Modifier().setfixedWidth(16).setHeight(0.75).align(Align::RIGHT | Align::CENTER_Y),
                                        sf::Color::White,
                                        sf::Color::Black,
                                        "master_slider"
                                    ),

                                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
                                }),
                            })
                        })
                    }),
                }),

                row(
                    Modifier()
                        .setWidth(1.f)
                        .setfixedHeight(256)
                        .setColor(sf::Color(200, 200, 200))
                        .align(Align::BOTTOM),
                contains{
                    button(
                        Modifier()
                            .align(Align::CENTER_X | Align::CENTER_Y)
                            .setfixedWidth(256)
                            .setfixedHeight(128)
                            .setColor(sf::Color::Red),
                        ButtonStyle::Pill, 
                        "KICK", 
                        "assets/fonts/OpenSans-Regular.ttf", 
                        sf::Color(230, 230, 230),
                        "KICK"
                    ),
                    spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
                    button(
                        Modifier()
                            .align(Align::CENTER_X | Align::CENTER_Y)
                            .setfixedWidth(256)
                            .setfixedHeight(128)
                            .setColor(sf::Color::Red),
                        ButtonStyle::Pill, 
                        "SNARE", 
                        "assets/fonts/OpenSans-Regular.ttf", 
                        sf::Color(230, 230, 230),
                        "SNARE"
                    ),
                }),
            })
        }), "base" }
    });

    bool prevNumpad1 = false;
    bool prevNumpad3 = false;

    bool fileLoaded = false;
    juce::FileChooser chooser("Select audio file", juce::File(), "*.wav;*.mp3;*.flac");
    std::unique_ptr<juce::AudioFormatReader> reader;

    while (ui.isRunning()) {
        if (buttons["KICK"]->isClicked()) {
            engine.setPosition(0.0);
            engine.play();
        }

        if (buttons["SNARE"]->isClicked()) {
            engine.setPosition(25.0);
            engine.play();
        }

        if (buttons["SAVE"]->isClicked()) {
            std::cout << "Save button clicked!\n";
        }

        if (buttons["LOAD"]->isClicked()) {
            chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    if (f.existsAsFile()) {
                        reader.reset(engine.formatManager.createReaderFor(f));
                        if (reader) {
                            engine.newComposition(f.getFileNameWithoutExtension().toStdString());
                            engine.addTrack("Track 1");
                            double dur = reader->lengthInSamples / reader->sampleRate;
                            engine.getTrack(0)->addClip(AudioClip(f, 0.0, 0.0, dur));
                            fileLoaded = true;
                        }
                    }
                }
            );
        }

        bool currNumpad1 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Numpad1);
        bool currNumpad3 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Numpad3);

        if (currNumpad1 && !prevNumpad1) {
            engine.setPosition(0.0);
            engine.play();
        }
        if (currNumpad3 && !prevNumpad3) {
            engine.setPosition(25.0);
            engine.play();
        }

        prevNumpad1 = currNumpad1;
        prevNumpad3 = currNumpad3;

        ui.update();
        ui.render();

    }
}

