#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <SFML/Graphics/Color.hpp>
#include "Engine.hpp"

struct TrackData {
    std::string name;
    float volume = 1.0f;
    float pan = 0.0f;
    std::vector<AudioClip> clips;

    TrackData(const std::string& trackName = "Master")
        : name(trackName) {}
};

struct UIState {
    std::string fileBrowserDirectory = "";
    std::string selectedTrackName   = "Master";
    int         trackCount          = 0;
    int         autoSaveIntervalSeconds = 300; // default 5 minutes

    TrackData masterTrack{"Master"};
    std::unordered_map<std::string, TrackData> tracks;
};

struct UIResources {
    std::string openSansFont;
    // Add more resources as needed
};

// Color constants
static const sf::Color pastel_red         (255, 179, 186);
static const sf::Color pastel_green       (186, 255, 201);
static const sf::Color button_color       (230,   0,   0);
static const sf::Color track_color        (180, 180, 180);
static const sf::Color master_track_color (120, 120, 120);
static const sf::Color mute_color         (230,   0,   0);