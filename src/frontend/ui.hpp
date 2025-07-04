#pragma once

#include <fstream>

#ifndef UI_HPP
#define UI_HPP

#include "UILO.hpp"

// Expose specific parts of UILO if needed
using uilo::Button;
using uilo::Slider;
using uilo::Page;
using uilo::Container;

#endif // UI_HPP

#include "Engine.hpp"
#include "UIHelpers.hpp"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

using namespace uilo;

class MyComponent : public juce::Component, public juce::Button::Listener
{
public:
    MyComponent()
    {
        // Initialize button
        myButton = std::make_unique<juce::TextButton>("Click Me");

        // Add button to the parent component
        addAndMakeVisible(myButton.get());

        // Attach listener
        myButton->addListener(this);

        // Set button properties
        myButton->setBounds(50, 50, 100, 30); // Example bounds
    }

    ~MyComponent() override
    {
        // Remove listener to avoid dangling references
        myButton->removeListener(this);
    }

    void resized() override
    {
        // Update button bounds if needed
        myButton->setBounds(getLocalBounds().reduced(10));
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == myButton.get())
        {
            juce::Logger::writeToLog("Button clicked!");
        }
    }

private:
    std::unique_ptr<juce::TextButton> myButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyComponent)
};

void application()
{
    Engine engine;
    engine.newComposition("test");

    // Add tracks
    engine.addTrack("Track 1");
    engine.addTrack("Track 2");

    auto* kick = engine.getTrack(0);
    auto* snare = engine.getTrack(1);

    // Storage for last non-zero volume (for mute toggling)
    static float previousVolume[2] = {1.0f, 1.0f};

    // Preload two sample clips
    juce::File kickSample("assets/test_samples/kick.wav");
    juce::File snareSample("assets/test_samples/snare.wav");

    AudioClip kickClip(
        kickSample,                     // Sample file
        0.0,                            // Composition start
        0.0,                            // Sample offset
        kickSample.getSize() / 44100.0  // Duration
    );
    AudioClip snareClip(
        snareSample,
        25.0,
        0.0,
        snareSample.getSize() / 44100.0
    );

    kick->addClip(kickClip);
    snare->addClip(snareClip);

    // Build the actual UILO layout
    UILO ui("MULO", {{
        page({
            column(Modifier(), contains{

                // ─── File Loader / Save Row ─────────────────────────────────────────
                row(
                    Modifier().setWidth(1.f).setfixedHeight(64).setColor(sf::Color(200, 200, 200)),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier()
                                .setfixedWidth(96)
                                .setfixedHeight(40)
                                .align(Align::LEFT | Align::CENTER_Y)
                                .setColor(sf::Color(200, 0, 0))
                                .onClick([&]() {
                                    // Load logic here
                                }),
                            ButtonStyle::Pill,
                            "Load",
                            "assets/fonts/OpenSans-Regular.ttf",
                            sf::Color(255, 255, 255),
                            "LOAD"
                        ),

                        spacer(Modifier().setfixedWidth(16)),

                        button(
                            Modifier()
                                .setfixedWidth(96)
                                .setfixedHeight(40)
                                .align(Align::RIGHT | Align::CENTER_Y)
                                .setColor(sf::Color(200, 0, 0))
                                .onClick([&]() {
                                    // Save logic here
                                }),
                            ButtonStyle::Pill,
                            "Save",
                            "assets/fonts/OpenSans-Regular.ttf",
                            sf::Color(255, 255, 255),
                            "SAVE"
                        ),

                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    }
                ),

                // ─── Transport Row ───────────────────────────────────────────────────
                row(
                    Modifier().setWidth(1.f).setfixedHeight(64).setColor(sf::Color(200, 200, 200)),
                    contains{
                        button(
                            Modifier()
                                .setfixedWidth(96)
                                .setfixedHeight(40)
                                .align(Align::LEFT | Align::CENTER_Y)
                                .setColor(sf::Color(0, 200, 0))
                                .onClick([&]() { engine.play(); }),
                            ButtonStyle::Rect,
                            "PLAY",
                            "assets/fonts/OpenSans-Regular.ttf",
                            sf::Color(255, 255, 255),
                            "PLAY"
                        ),

                        spacer(Modifier().setfixedWidth(16)),

                        button(
                            Modifier()
                                .setfixedWidth(96)
                                .setfixedHeight(40)
                                .align(Align::LEFT | Align::CENTER_Y)
                                .setColor(sf::Color(200, 0, 0))
                                .onClick([&]() { engine.pause(); }),
                            ButtonStyle::Rect,
                            "PAUSE",
                            "assets/fonts/OpenSans-Regular.ttf",
                            sf::Color(255, 255, 255),
                            "PAUSE"
                        ),

                        spacer(Modifier().setfixedWidth(16)),

                        slider(
                            Modifier()
                                .setWidth(1.f)
                                .setfixedHeight(20)
                                .align(Align::CENTER_Y),
                            sf::Color(255, 255, 255),   // knobColor
                            sf::Color(0, 0, 0),         // barColor
                            "TRANSPORT_SLIDER"
                        )
                    }
                ),

                // ─── Mixer UI ────────────────────────────────────────────────────────
                row(
                    Modifier().setWidth(1.f).setfixedHeight(256).setColor(sf::Color(180, 180, 180)),
                    contains{

                        column(
                            Modifier().setWidth(0.5f).setColor(sf::Color(155, 155, 155)),
                            contains{
                                text(
                                    Modifier().align(Align::CENTER_X),
                                    "Track 1",
                                    "assets/fonts/OpenSans-Regular.ttf"
                                ),

                                slider(
                                    Modifier()
                                        .setWidth(0.8f)
                                        .setHeight(0.6f)
                                        .align(Align::CENTER_X),
                                    sf::Color(255, 255, 255),
                                    sf::Color(0, 0, 0),
                                    "TRACK_1_VOLUME"
                                ),

                                button(
                                    Modifier()
                                        .setfixedWidth(64)
                                        .setfixedHeight(40)
                                        .align(Align::CENTER_X)
                                        .setColor(sf::Color(200, 0, 0))
                                        .onClick([&]() {
                                            auto* t = engine.getTrack(0);
                                            float v = t->getVolume();
                                            t->setVolume(v > 0 ? 0.0f : 1.0f);
                                        }),
                                    ButtonStyle::Rect,
                                    "Mute",
                                    "assets/fonts/OpenSans-Regular.ttf",
                                    sf::Color(255, 255, 255),
                                    "TRACK_1_MUTE"
                                )
                            }
                        ),

                        column(
                            Modifier().setWidth(0.5f).setColor(sf::Color(155, 155, 155)),
                            contains{
                                text(
                                    Modifier().align(Align::CENTER_X),
                                    "Track 2",
                                    "assets/fonts/OpenSans-Regular.ttf"
                                ),

                                slider(
                                    Modifier()
                                        .setWidth(0.8f)
                                        .setHeight(0.6f)
                                        .align(Align::CENTER_X),
                                    sf::Color(255, 255, 255),
                                    sf::Color(0, 0, 0),
                                    "TRACK_2_VOLUME"
                                ),

                                button(
                                    Modifier()
                                        .setfixedWidth(64)
                                        .setfixedHeight(40)
                                        .align(Align::CENTER_X)
                                        .setColor(sf::Color(200, 0, 0))
                                        .onClick([&]() {
                                            auto* t = engine.getTrack(1);
                                            float v = t->getVolume();
                                            t->setVolume(v > 0 ? 0.0f : 1.0f);
                                        }),
                                    ButtonStyle::Rect,
                                    "Mute",
                                    "assets/fonts/OpenSans-Regular.ttf",
                                    sf::Color(255, 255, 255),
                                    "TRACK_2_MUTE"
                                )
                            }
                        )
                    }
                )
            })
        }),
        "base"
    }});

    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::FileChooser chooser("Select audio file", juce::File(), "*.wav;*.mp3;*.flac");

    while (ui.isRunning())
    {
        // File Load Handler
        if (buttons["LOAD"]->isClicked())
        {
            chooser.launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    if (f.existsAsFile())
                    {
                        reader.reset(engine.formatManager.createReaderFor(f));
                        if (reader)
                        {
                            engine.newComposition(f.getFileNameWithoutExtension().toStdString());
                            kick->addClip(AudioClip(
                                f, 0.0, 0.0,
                                reader->lengthInSamples / reader->sampleRate
                            ));
                        }
                    }
                }
            );
        }

        // Transport Controls
        if (buttons["PLAY"]->isClicked()) engine.play();
        if (buttons["PAUSE"]->isClicked()) engine.pause();

        // Mixer Handlers
        if (sliders.count("TRACK_1_VOLUME"))
        {
            engine.getTrack(0)->setVolume(sliders["TRACK_1_VOLUME"]->getValue());
        }
        if (buttons["TRACK_1_MUTE"]->isClicked())
        {
            auto* t = engine.getTrack(0);
            float v = t->getVolume();
            t->setVolume(v > 0.0f ? 0.0f : previousVolume[0]);
        }

        if (sliders.count("TRACK_2_VOLUME"))
        {
            engine.getTrack(1)->setVolume(sliders["TRACK_2_VOLUME"]->getValue());
        }
        if (buttons["TRACK_2_MUTE"]->isClicked())
        {
            auto* t = engine.getTrack(1);
            float v = t->getVolume();
            t->setVolume(v > 0.0f ? 0.0f : previousVolume[1]);
        }

        // Transport Slider Update
        double pos = engine.getPosition();
        double dur = reader ? reader->lengthInSamples / reader->sampleRate : 1.0;
        sliders["TRANSPORT_SLIDER"]->setValue(static_cast<float>(pos / dur));

        ui.update();
        ui.render();
    }
}
