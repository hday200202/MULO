#pragma once

#include <fstream>

#include "UILO.hpp"
#include "Engine.hpp"
#include "UIHelpers.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

using namespace uilo;

void application() {
    Engine engine;
    engine.newComposition("untitled");

    initUIResources();

    UILO ui("MULO", {{
        page({
            column(
                Modifier(),
            contains{
                topRow(),
                browserAndTimeline(),
                fxRack(),
            }),
        }), "base" }
    });

    float masterVolume = sliders["Master_volume_slider"]->getValue();

    while (ui.isRunning()) {
        if (buttons["select_directory"]->isClicked()) {
            std::string dir = selectDirectory();
            if (!dir.empty()) {
                uiState.file_browser_directory = dir;
                std::cout << "Selected directory: " << dir << std::endl;
            }
        }

        if (buttons["new_track"]->isClicked()) {
            newTrack(engine, uiState);
            std::cout << "New track added. Total tracks: " << uiState.track_count << std::endl;
        }

        if (sliders["Master_volume_slider"]->getValue() != masterVolume) {
            masterVolume = sliders["Master_volume_slider"]->getValue();
            std::cout << "Master volume changed to: " << floatToDecibels(masterVolume) << " db" << std::endl;
        }

        ui.update();
        ui.render();
    }
}