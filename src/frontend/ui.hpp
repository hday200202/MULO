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
                        .setfixedHeight(64)
                        .onClick([&]() {
                            std::cout << "Bottom row clicked!" << std::endl;
                        }),
                contains{

                }),

                column(
                    Modifier()
                        .setColor(sf::Color(200, 200, 200))
                        .setfixedWidth(256)
                        .setHeight(1.f)
                        .align(Align::LEFT)
                        .onClick([&]() {
                            std::cout << "Left column clicked!" << std::endl;
                        }),
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
                        .setfixedHeight(256)
                        .onClick([&]() {
                            std::cout << "Bottom row clicked!" << std::endl;
                        }),
                contains{
                    slider(
                        Modifier()
                            .setfixedHeight(180)
                            .setfixedWidth(25)
                            .align(Align::CENTER_Y),
                        sf::Color::White,
                        sf::Color::Black
                    ),

                })
            })
        }), "base" }
    });

    while (ui.isRunning()) {
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