#pragma once

#include "UILO.hpp"
using namespace uilo;

void loop() {
    UILO ui("MULO", {{
        page({
            row(
                Modifier()
                    .setColor(sf::Color::White)
                    .setHeight(1.f)
                    .setWidth(1.f),
            contains{
                // Add your UI elements here
            })
        }), "base" }
    });

    while (ui.isRunning()) {
        ui.update();
        ui.render();
    }
}