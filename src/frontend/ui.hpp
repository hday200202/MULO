#pragma once

#include "UILO.hpp"
#include "Engine.hpp"

using namespace uilo;

void application() {
    Engine engine;
    engine.newComposition("test");

    // Add one track
    engine.addTrack("Track 1");
    auto* track = engine.getTrack(0);

    // Load a clip starting at 0s
    juce::File sampleFile("assets/test_samples/kick.wav");
    AudioClip clip(sampleFile, 0.0, 0.0, sampleFile.getSize() / 44100.0);
    track->addClip(clip);

    UILO ui("MULO", {{
        page({
            column(
                Modifier()
                    .setColor(sf::Color(25, 25, 25))
                    .setHeight(1.f)
                    .setWidth(1.f),
            contains{
                row(
                    Modifier()
                        .setColor(sf::Color(210, 210, 210))
                        .setWidth(1.f)
                        .setfixedHeight(64),
                contains{}),

                column(
                    Modifier()
                        .setColor(sf::Color(200, 200, 200))
                        .setfixedWidth(256)
                        .setHeight(1.f)
                        .align(Align::LEFT),
                contains{
                    spacer(Modifier().setfixedHeight(16)),

                    text(
                        Modifier()
                            .setColor(sf::Color::Black)
                            .setfixedHeight(16)
                            .align(Align::TOP | Align::CENTER_X),
                        "FILES",
                        "assets/fonts/SpaceMono-Regular.ttf"
                    ),

                    spacer(Modifier().setfixedHeight(16)),
                }),

                row(
                    Modifier()
                        .setColor(sf::Color(210, 210, 210))
                        .setWidth(1.f)
                        .setfixedHeight(256),
                contains{
                    column(
                        Modifier().setfixedWidth(100).setHeight(1.f).align(Align::CENTER_X | Align::CENTER_Y),
                    contains{
                        spacer(Modifier().setfixedHeight(16)),
                        slider(
                            Modifier()
                                .setfixedHeight(180)
                                .setfixedWidth(25)
                                .align(Align::TOP | Align::CENTER_X),
                            sf::Color::White,
                            sf::Color::Black,
                            "My Slider"
                        ),

                        spacer(Modifier().setfixedHeight(16)),

                    }),

                    button(
                        Modifier()
                            .setfixedHeight(64)
                            .setfixedWidth(64)
                            .align(Align::RIGHT | Align::CENTER_Y)
                            .setColor(sf::Color::Red),
                        ButtonStyle::Pill,
                        "OK",
                        "assets/fonts/SpaceMono-Regular.ttf",
                        sf::Color::White,
                        "My Button"
                    ),

                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
                })
            })
        }), "base" }
    });

    while (ui.isRunning()) {
        if (buttons["My Button"]->isClicked())
            engine.play();
        ui.update();
        ui.render();
    }
}