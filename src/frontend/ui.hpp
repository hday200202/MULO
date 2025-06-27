#pragma once

#include "UILO.hpp"
using namespace uilo;

void loop() {
    UILO ui("MULO", {{
        page({
            column(
                Modifier()
                    .setColor(sf::Color(25, 25, 25))
                    .setHeight(1.f)
                    .setWidth(1.f),
            contains{
                // Add your UI elements here
                row(
                    Modifier()
                        .setColor(sf::Color(210, 210, 210))
                        .setWidth(1.f)
                        .setfixedHeight(64),
                contains{

                }),

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
                    slider(
                        Modifier()
                            .setfixedHeight(180)
                            .setfixedWidth(25)
                            .align(Align::CENTER_Y | Align::CENTER_X),
                        sf::Color::White,
                        sf::Color::Black,
                        "My Slider"
                    ),

                    button(
                        Modifier()
                            .setfixedHeight(64)
                            .setfixedWidth(256)
                            .align(Align::CENTER_Y | Align::CENTER_X)
                            .setColor(sf::Color::Red),
                        ButtonStyle::Pill,
                        "My Button",
                        "assets/fonts/SpaceMono-Regular.ttf",
                        sf::Color::White, // <-- text color
                        "My Button"
                    )

                })
            })
        }), "base" }
    });

    float sliderVal = sliders["My Slider"]->getValue();
    while (ui.isRunning()) {
        if (sliders["My Slider"]->getValue() != sliderVal) {
            sliderVal = sliders["My Slider"]->getValue();
            std::cout << sliderVal << std::endl;
        }

        if (buttons["My Button"]->isClicked()) {
            std::cout << "Button clicked!" << std::endl;
        }
        
        ui.update();
        ui.render();
    }

    // Text(
    //     Modifier()
    //         .setColor(sf::Color::Black)
    //         .setfixedHeight(32)
    //         .setfixedWidth(256)
    //         .align(Align::CENTER_X | Align::CENTER_Y),
    //     "Left Column",
    //     "assets/fonts/SpaceMono-Regular.ttf"
    // );
}