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
                
            })
        }), "base" }
    });

    while (ui.isRunning()) {
        ui.update();
        ui.render();
    }
}