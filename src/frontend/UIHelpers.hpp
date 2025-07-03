#pragma once

#include "UILO.hpp"
#include "Engine.hpp"
#include <juce_gui_extra/juce_gui_extra.h>
#include <string>

using namespace uilo;

struct UIState {
    std::string file_browser_directory = "";
    int track_count = 0;
    // Add more state variables as needed
};

struct UIResources {
    std::string openSansFont = "assets/fonts/OpenSans-Regular.ttf";
    // Add more resources as needed
};

Row* topRow();
Row* browserAndTimeline();
Row* fxRack();
Row* track(const std::string& trackName = "", Align alignment = Align::LEFT | Align::TOP);
std::string selectDirectory();
std::string selectFile(std::initializer_list<std::string> filters = {"*.wav", "*.mp3", "*.flac"});
void newTrack(Engine& engine, UIState& uiState);

UIState uiState;
UIResources resources;

Row* topRow() {
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
            "Load", 
            resources.openSansFont, 
            sf::Color(230, 230, 230),
            "LOAD"
        ),

        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setHeight(.75f).setfixedWidth(96).setColor(sf::Color::Red),
            ButtonStyle::Pill,
            "Save",
            resources.openSansFont,
            sf::Color::White,
            "SAVE"
        ),

        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
    });
}

Row* browserAndTimeline() {
    return row(
        Modifier().setWidth(1.f).setHeight(1.f),
    contains{
        column(
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
                track("Master", Align::BOTTOM | Align::LEFT),
            })
        }),
    });
}

Row* fxRack() {
    return row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(256)
            .setColor(sf::Color(200, 200, 200))
            .align(Align::BOTTOM),
    contains{
        
    });
}

Row* track(const std::string& trackName, Align alignment) {
    return row(
        Modifier()
            .setColor(sf::Color(120, 120, 120))
            .setfixedHeight(96)
            .align(alignment),
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
                trackName,
                resources.openSansFont
            ),

            slider(
                Modifier().setfixedWidth(16).setHeight(0.75).align(Align::RIGHT | Align::CENTER_Y),
                sf::Color::White,
                sf::Color::Black,
                trackName + "_volume_slider"
            ),

            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),
    });
}

std::string selectDirectory() {
    juce::FileChooser chooser("Select directory", juce::File(), "*");
    if (chooser.browseForDirectory()) {
        return chooser.getResult().getFullPathName().toStdString();
    }
    return "";
}

std::string selectFile(std::initializer_list<std::string> filters) {
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

void newTrack(Engine& engine, UIState& uiState) {
    engine.addTrack("Track_" + std::to_string(uiState.track_count++));
    std::string samplePath = selectFile({"*.wav", "*.mp3", "*.flac"});
    if (!samplePath.empty()) {
        engine.getTrack(uiState.track_count - 1)->addClip(AudioClip(juce::File(samplePath), 0.0, 0.0, 0.0, 1.0f));
        std::cout << "Loaded sample: " << samplePath << " into Track " << (uiState.track_count - 1) << std::endl;
    } else {
        std::cout << "No sample selected for Track " << (uiState.track_count - 1) << std::endl;
    }
}