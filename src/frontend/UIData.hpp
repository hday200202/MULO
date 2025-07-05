#pragma once

#include <string>
#include <vector>
#include <unordered_map>

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
    std::string selectedTrackName = "Master";
    int trackCount = 0;

    TrackData masterTrack{"Master"};

    std::unordered_map<std::string, TrackData> tracks;
};

struct UIResources {
    std::string openSansFont;
    // Add more resources as needed
};