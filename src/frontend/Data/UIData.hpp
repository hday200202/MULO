
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include "Engine.hpp"
#include "../DebugConfig.hpp"
#include "UILO/UILO.hpp"

using json = nlohmann::json;

struct UITheme;

struct UIState {
    std::string fileBrowserDirectory = "";
    std::string vstDirecory = "";
    std::vector<std::string> vstDirectories = {};
    std::string saveDirectory = "";
    std::string selectedTheme = "Dark";

    float timelineZoomLevel = 1.f;
    float uiScale = 1.f;
    double sampleRate = 44100.0;
    int autoSaveIntervalSeconds = 300;
    bool settingsShown = false;
    bool marketplaceShown = false;
    bool enableAutoVSTScan = false;
    
    std::string compositionName = "untitled";
    std::string bpmStr = "120";
    std::string sampleRateStr = "44100";
    
    bool xResizing = false;
    bool yResizing = false;

    inline void printUIState() {
        DEBUG_PRINT("  [File Browser Dir] " << fileBrowserDirectory);
        DEBUG_PRINT("     [VST Directory] " << vstDirecory);
        DEBUG_PRINT("          [Save Dir] " << saveDirectory);
        DEBUG_PRINT("          [UI Theme] " << selectedTheme);
        DEBUG_PRINT("       [Sample Rate] " << sampleRate);
        DEBUG_PRINT("[Auto Save Interval] " << autoSaveIntervalSeconds);
    }

    inline std::string getExecutableDirectory() {
        try {
            std::filesystem::path exePath = std::filesystem::current_path();
            return exePath.string();
        } catch (const std::exception& e) {
            DEBUG_PRINT("Error getting executable directory: " << e.what());
            return ".";
        }
    }
};

struct UIResources {
    std::string dejavuSansFont;
    std::string spaceMonoFont;
    std::string ubuntuBoldFont;
    std::string ubuntuMonoFont;
    std::string ubuntuMonoBoldFont;

    sf::Image playIcon;
    sf::Image pauseIcon;
    sf::Image settingsIcon;
    sf::Image pianoRollIcon;
    sf::Image loadIcon;
    sf::Image saveIcon;
    sf::Image exportIcon;
    sf::Image folderIcon;
    sf::Image openFolderIcon;
    sf::Image pluginFileIcon;
    sf::Image audioFileIcon;
    sf::Image metronomeIcon;
    sf::Image mixerIcon;
    sf::Image storeIcon;
    sf::Image fileIcon;
    sf::Image automationIcon;
    sf::Image collabIcon;
    sf::Image loginIcon;
    sf::Image muloIcon;

    UITheme* activeTheme = nullptr;
    // Add more resources as needed
};

struct ComponentLayoutData {
    uilo::Container* parent = nullptr;
    uilo::Align alignment = uilo::Align::LEFT;
    std::string relativeTo = "";
};

// Helper function to convert hex string to sf::Color
inline sf::Color hexToColor(const std::string& hex) {
    std::string hexStr = hex;
    if (!hexStr.empty() && hexStr[0] == '#')
        hexStr = hexStr.substr(1);
    
    unsigned long long hexValue = 0;
    try {hexValue = std::stoull(hexStr, nullptr, 16);}
    catch (...) {return sf::Color::Black;}
    
    unsigned char r, g, b, a = 255;
    
    // Check if it includes alpha (8 hex digits / 4 bytes)
    if (hexStr.length() == 8) {
        r = (hexValue >> 24) & 0xFF;
        g = (hexValue >> 16) & 0xFF;
        b = (hexValue >> 8) & 0xFF;
        a = hexValue & 0xFF;
    } else {
        // 6 hex digits / 3 bytes (no alpha)
        r = (hexValue >> 16) & 0xFF;
        g = (hexValue >> 8) & 0xFF;
        b = hexValue & 0xFF;
    }
    
    return sf::Color(r, g, b, a);
}

// Theme registry
inline std::unordered_map<std::string, const UITheme*>& getThemeRegistry() {
    static std::unordered_map<std::string, const UITheme*> registry;
    return registry;
}

inline std::vector<std::string>& getThemeNames() {
    static std::vector<std::string> names;
    return names;
}

struct UITheme {
    std::string name;
    sf::Color button_color;
    sf::Color track_color;
    sf::Color track_row_color;
    sf::Color master_track_color;
    sf::Color mute_color;
    sf::Color foreground_color;
    sf::Color primary_text_color;
    sf::Color secondary_text_color;
    sf::Color not_muted_color;
    sf::Color middle_color;
    sf::Color alt_button_color;
    sf::Color white;
    sf::Color black;
    sf::Color slider_knob_color;
    sf::Color slider_bar_color;
    sf::Color clip_color;
    sf::Color line_color;
    sf::Color wave_form_color;
    sf::Color selected_track_color;
    sf::Color automation_lane_color;
    sf::Color automation_label_color;
    
    UITheme(
        const std::string& themeName,
        sf::Color btn = sf::Color::Red,
        sf::Color track = sf::Color(155, 155, 155),
        sf::Color trackRow = sf::Color(120, 120, 120),
        sf::Color masterTrack = sf::Color(155, 155, 155),
        sf::Color mute = sf::Color::Red,
        sf::Color fg = sf::Color(200, 200, 200),
        sf::Color primaryText = sf::Color::Black,
        sf::Color secondaryText = sf::Color::White,
        sf::Color notMuted = sf::Color(50, 50, 50),
        sf::Color middle = sf::Color(100, 100, 100),
        sf::Color altBtn = sf::Color(120, 120, 120),
        sf::Color w = sf::Color::White,
        sf::Color b = sf::Color::Black,
        sf::Color sliderKnob = sf::Color::White,
        sf::Color sliderBar = sf::Color::Black,
        sf::Color clip = sf::Color(100, 150, 200),
        sf::Color line = sf::Color(80, 80, 80),
        sf::Color waveform = sf::Color(0, 150, 255),
        sf::Color selectedTrack = sf::Color(100, 150, 200),
        sf::Color automationLane = sf::Color(100, 100, 100),
        sf::Color automationLabel = sf::Color(140, 140, 140)
    ) : name(themeName), button_color(btn), track_color(track), track_row_color(trackRow), master_track_color(masterTrack),
        mute_color(mute), foreground_color(fg), primary_text_color(primaryText), secondary_text_color(secondaryText),
        not_muted_color(notMuted), middle_color(middle), alt_button_color(altBtn), white(w), black(b),
        slider_knob_color(sliderKnob), slider_bar_color(sliderBar), clip_color(clip), line_color(line), 
        wave_form_color(waveform), selected_track_color(selectedTrack), automation_lane_color(automationLane),
        automation_label_color(automationLabel) {
        // Register this theme only if it's not already registered
        auto& registry = getThemeRegistry();
        if (registry.find(themeName) == registry.end()) {
            registry[themeName] = this;
            getThemeNames().push_back(themeName);
        }
    }
};

namespace Themes {
    // MULO Default Themes
    const UITheme Dark = UITheme(
        "Dark Default",                       // theme name
        hexToColor("#55738CFF"),      // buttonColor
        hexToColor("#3C3C3CFF"),      // trackColor
        hexToColor("#2D2D2DFF"),      // trackRowColor
        hexToColor("#505050FF"),      // masterTrackColor
        hexToColor("#8C4646FF"),      // muteColor
        hexToColor("#464646FF"),      // foregroundColor
        hexToColor("#E6E6E6FF"),      // primaryTextColor
        hexToColor("#E6E6E6FF"),      // secondaryTextColor
        hexToColor("#1E1E1EFF"),      // notMutedColor
        hexToColor("#282828FF"),      // middleColor
        hexToColor("#323232FF"),      // altButtonColor
        hexToColor("#FFFFFFFF"),      // white
        hexToColor("#141414FF"),      // black
        hexToColor("#FFFFFFFF"),      // sliderKnobColor
        hexToColor("#1E1E1EFF"),      // sliderBarColor
        hexToColor("#5A78A0FF"),      // clipColor
        hexToColor("#646464FF"),      // lineColor
        hexToColor("#FFFFFFFF"),      // waveformColor
        hexToColor("#5A78A0FF"),      // selectedTrackColor
        hexToColor("#232323FF"),      // automationLaneColor
        hexToColor("#323232FF")       // automationLabelColor
    );

    const UITheme Light = UITheme(
        "Light Default",                       // theme name
        hexToColor("#84a4cdff"),      // buttonColor
        hexToColor("#dbdbdbff"),      // trackColor
        hexToColor("#959595ff"),      // trackRowColor
        hexToColor("#b8b8b8ff"),      // masterTrackColor
        hexToColor("#8C4646FF"),      // muteColor
        hexToColor("#eeeeeeff"),      // foregroundColor
        hexToColor("#2a2a2aff"),      // primaryTextColor
        hexToColor("#d2d2d2ff"),      // secondaryTextColor
        hexToColor("#1E1E1EFF"),      // notMutedColor
        hexToColor("#c0c0c0ff"),      // middleColor
        hexToColor("#909090ff"),      // altButtonColor
        hexToColor("#FFFFFFFF"),      // white
        hexToColor("#141414FF"),      // black
        hexToColor("#2a2a2aff"),      // sliderKnobColor
        hexToColor("#232323ff"),      // sliderBarColor
        hexToColor("#84a4cdff"),      // clipColor
        hexToColor("#eeeeeeff"),      // lineColor
        hexToColor("#ffffffff"),      // waveformColor
        hexToColor("#84a4cdff"),      // selectedTrackColor
        hexToColor("#959595ff"),      // automationLaneColor
        hexToColor("#84a4cdff")       // automationLabelColor
    );

    const UITheme DarkForest = UITheme(
        "Dark Forest",                // theme name
        hexToColor("#4D7C5Aff"),      // buttonColor - muted green
        hexToColor("#3A4A3Cff"),      // trackColor - dark green-gray
        hexToColor("#2B352Dff"),      // trackRowColor - darker green-gray
        hexToColor("#4A5A4Cff"),      // masterTrackColor - medium green-gray
        hexToColor("#8C4646ff"),      // muteColor - keep red
        hexToColor("#424A44ff"),      // foregroundColor - dark green-tinted
        hexToColor("#D8E6DAff"),      // primaryTextColor - light green-tinted
        hexToColor("#D8E6DAff"),      // secondaryTextColor - light green-tinted
        hexToColor("#1C241Eff"),      // notMutedColor - very dark green
        hexToColor("#343C36ff"),      // middleColor - dark green-gray
        hexToColor("#3E4A40ff"),      // altButtonColor - muted green-gray
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#141814ff"),      // black - slightly green-tinted
        hexToColor("#D8E6DAff"),      // sliderKnobColor - light green-tinted
        hexToColor("#1C241Eff"),      // sliderBarColor - dark green
        hexToColor("#4D7C5Aff"),      // clipColor - muted green
        hexToColor("#586858ff"),      // lineColor - medium green-gray
        hexToColor("#D8E6DAff"),      // waveformColor - light green-tinted
        hexToColor("#4D7C5Aff"),      // selectedTrackColor - muted green
        hexToColor("#2C362Eff"),      // automationLaneColor - darker green
        hexToColor("#3A4A3Cff")       // automationLabelColor - dark green-gray
    );

    const UITheme LightForest = UITheme(
        "Light Forest",               // theme name
        hexToColor("#4D7C5Aff"),      // buttonColor - muted green
        hexToColor("#D5E8D8ff"),      // trackColor - light green-gray
        hexToColor("#B8D4BCff"),      // trackRowColor - lighter green-gray
        hexToColor("#C8DFCCff"),      // masterTrackColor - medium light green
        hexToColor("#C96B6Bff"),      // muteColor - lighter red
        hexToColor("#E0ECE2ff"),      // foregroundColor - light green-tinted
        hexToColor("#1C2A1Eff"),      // primaryTextColor - dark green-tinted
        hexToColor("#3C4A3Eff"),      // secondaryTextColor - dark green-tinted
        hexToColor("#F5F9F6ff"),      // notMutedColor - very light green
        hexToColor("#CFE0D2ff"),      // middleColor - light green-gray
        hexToColor("#C5D8C8ff"),      // altButtonColor - light green-gray
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#1C241Eff"),      // black - green-tinted black
        hexToColor("#1C2A1Eff"),      // sliderKnobColor - dark green-tinted
        hexToColor("#F5F9F6ff"),      // sliderBarColor - very light green
        hexToColor("#4D7C5Aff"),      // clipColor - muted green
        hexToColor("#E0ECE2ff"),      // lineColor - medium green-gray
        hexToColor("#E0ECE2ff"),      // waveformColor - dark green-tinted
        hexToColor("#4D7C5Aff"),      // selectedTrackColor - muted green
        hexToColor("#B8D4BCff"),      // automationLaneColor - light green
        hexToColor("#4D7C5Aff")       // automationLabelColor - light green-gray
    );

    const UITheme DarkOcean = UITheme(
        "Dark Ocean",                      // theme name
        hexToColor("#4A7BA7ff"),      // buttonColor - ocean blue
        hexToColor("#2D3E50ff"),      // trackColor - deep blue-gray
        hexToColor("#252F3Bff"),      // trackRowColor - darker blue-gray
        hexToColor("#3D5468ff"),      // masterTrackColor - medium blue
        hexToColor("#8C4646ff"),      // muteColor - red
        hexToColor("#34465Aff"),      // foregroundColor - dark blue-gray
        hexToColor("#D6E4F0ff"),      // primaryTextColor - light blue-tinted
        hexToColor("#D6E4F0ff"),      // secondaryTextColor - light blue-tinted
        hexToColor("#1A242Eff"),      // notMutedColor - very dark blue
        hexToColor("#2A3A48ff"),      // middleColor - dark blue-gray
        hexToColor("#3A4A5Eff"),      // altButtonColor - muted blue
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#12161Aff"),      // black - blue-tinted black
        hexToColor("#D6E4F0ff"),      // sliderKnobColor - light blue-tinted
        hexToColor("#1A242Eff"),      // sliderBarColor - dark blue
        hexToColor("#4A7BA7ff"),      // clipColor - ocean blue
        hexToColor("#506070ff"),      // lineColor - medium blue-gray
        hexToColor("#D6E4F0ff"),      // waveformColor - light blue-tinted
        hexToColor("#4A7BA7ff"),      // selectedTrackColor - ocean blue
        hexToColor("#222E3Aff"),      // automationLaneColor - darker blue
        hexToColor("#2D3E50ff")       // automationLabelColor - deep blue-gray
    );

    const UITheme LightOcean = UITheme(
        "Light Ocean",                // theme name
        hexToColor("#4A7BA7ff"),      // buttonColor - ocean blue
        hexToColor("#D3E3F2ff"),      // trackColor - light blue-gray
        hexToColor("#B5D2EAff"),      // trackRowColor - lighter blue-gray
        hexToColor("#C6DCEFff"),      // masterTrackColor - medium light blue
        hexToColor("#C96B6Bff"),      // muteColor - lighter red
        hexToColor("#DEE9F4ff"),      // foregroundColor - light blue-tinted
        hexToColor("#1A2838ff"),      // primaryTextColor - dark blue-tinted
        hexToColor("#3A4858ff"),      // secondaryTextColor - dark blue-tinted
        hexToColor("#F3F7FBff"),      // notMutedColor - very light blue
        hexToColor("#CEDFF0ff"),      // middleColor - light blue-gray
        hexToColor("#C4D6E8ff"),      // altButtonColor - light blue-gray
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#1A242Eff"),      // black - blue-tinted black
        hexToColor("#1A2838ff"),      // sliderKnobColor - dark blue-tinted
        hexToColor("#F3F7FBff"),      // sliderBarColor - very light blue
        hexToColor("#4A7BA7ff"),      // clipColor - ocean blue
        hexToColor("#DEE9F4ff"),      // lineColor - medium blue-gray
        hexToColor("#DEE9F4ff"),      // waveformColor - dark blue-tinted
        hexToColor("#4A7BA7ff"),      // selectedTrackColor - ocean blue
        hexToColor("#B5D2EAff"),      // automationLaneColor - light blue
        hexToColor("#4A7BA7ff")       // automationLabelColor - light blue-gray
    );

    const UITheme DarkSunset = UITheme(
        "Dark Sunset",                     // theme name
        hexToColor("#D9704Dff"),      // buttonColor - warm orange
        hexToColor("#4A3838ff"),      // trackColor - warm dark gray
        hexToColor("#3A2E2Eff"),      // trackRowColor - darker warm gray
        hexToColor("#5A4646ff"),      // masterTrackColor - warm medium gray
        hexToColor("#A84646ff"),      // muteColor - deeper red
        hexToColor("#524242ff"),      // foregroundColor - warm dark
        hexToColor("#F5E6D3ff"),      // primaryTextColor - warm cream
        hexToColor("#F5E6D3ff"),      // secondaryTextColor - warm cream
        hexToColor("#2A1E1Eff"),      // notMutedColor - very dark warm
        hexToColor("#423636ff"),      // middleColor - warm dark gray
        hexToColor("#4E3E3Eff"),      // altButtonColor - warm muted
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#1A1414ff"),      // black - warm black
        hexToColor("#F5E6D3ff"),      // sliderKnobColor - warm cream
        hexToColor("#2A1E1Eff"),      // sliderBarColor - dark warm
        hexToColor("#D9704Dff"),      // clipColor - warm orange
        hexToColor("#6E5858ff"),      // lineColor - warm medium
        hexToColor("#F5E6D3ff"),      // waveformColor - warm cream
        hexToColor("#D9704Dff"),      // selectedTrackColor - warm orange
        hexToColor("#342828ff"),      // automationLaneColor - darker warm
        hexToColor("#4A3838ff")       // automationLabelColor - warm dark gray
    );

    const UITheme LightSunset = UITheme(
        "Light Sunset",               // theme name
        hexToColor("#D9704Dff"),      // buttonColor - warm orange
        hexToColor("#F5E6DDff"),      // trackColor - light warm gray
        hexToColor("#E8D3C5ff"),      // trackRowColor - lighter warm gray
        hexToColor("#F0DDD0ff"),      // masterTrackColor - medium light warm
        hexToColor("#D96B6Bff"),      // muteColor - warm red
        hexToColor("#F8EEE6ff"),      // foregroundColor - light warm
        hexToColor("#2A1E1Eff"),      // primaryTextColor - dark warm
        hexToColor("#4A3E3Eff"),      // secondaryTextColor - dark warm
        hexToColor("#FFF9F6ff"),      // notMutedColor - very light warm
        hexToColor("#EFE0D5ff"),      // middleColor - light warm gray
        hexToColor("#E8D8CCff"),      // altButtonColor - light warm gray
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#2A1E1Eff"),      // black - warm black
        hexToColor("#2A1E1Eff"),      // sliderKnobColor - dark warm
        hexToColor("#FFF9F6ff"),      // sliderBarColor - very light warm
        hexToColor("#D9704Dff"),      // clipColor - warm orange
        hexToColor("#F8EEE6ff"),      // lineColor - medium warm
        hexToColor("#F8EEE6ff"),      // waveformColor - dark warm
        hexToColor("#D9704Dff"),      // selectedTrackColor - warm orange
        hexToColor("#E8D3C5ff"),      // automationLaneColor - light warm
        hexToColor("#D9704Dff")       // automationLabelColor - light warm gray
    );

    const UITheme DarkPurple = UITheme(
        "Dark Purple",                     // theme name
        hexToColor("#8B6BA8ff"),      // buttonColor - soft purple
        hexToColor("#3E3648ff"),      // trackColor - dark purple-gray
        hexToColor("#302838ff"),      // trackRowColor - darker purple-gray
        hexToColor("#4E4658ff"),      // masterTrackColor - medium purple
        hexToColor("#8C4646ff"),      // muteColor - red
        hexToColor("#443E4Eff"),      // foregroundColor - dark purple-tinted
        hexToColor("#E6DCEEff"),      // primaryTextColor - light purple-tinted
        hexToColor("#E6DCEEff"),      // secondaryTextColor - light purple-tinted
        hexToColor("#221E28ff"),      // notMutedColor - very dark purple
        hexToColor("#3A3442ff"),      // middleColor - dark purple-gray
        hexToColor("#4A4252ff"),      // altButtonColor - muted purple
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#18141Aff"),      // black - purple-tinted black
        hexToColor("#E6DCEEff"),      // sliderKnobColor - light purple-tinted
        hexToColor("#221E28ff"),      // sliderBarColor - dark purple
        hexToColor("#8B6BA8ff"),      // clipColor - soft purple
        hexToColor("#5E5266ff"),      // lineColor - medium purple-gray
        hexToColor("#E6DCEEff"),      // waveformColor - light purple-tinted
        hexToColor("#8B6BA8ff"),      // selectedTrackColor - soft purple
        hexToColor("#2C2834ff"),      // automationLaneColor - darker purple
        hexToColor("#3E3648ff")       // automationLabelColor - dark purple-gray
    );

    const UITheme LightPurple = UITheme(
        "Light Purple",               // theme name
        hexToColor("#8B6BA8ff"),      // buttonColor - soft purple
        hexToColor("#E8DDF4ff"),      // trackColor - light purple-gray
        hexToColor("#D5C4E8ff"),      // trackRowColor - lighter purple-gray
        hexToColor("#DFD3EFff"),      // masterTrackColor - medium light purple
        hexToColor("#C96B6Bff"),      // muteColor - lighter red
        hexToColor("#EDE3F6ff"),      // foregroundColor - light purple-tinted
        hexToColor("#241828ff"),      // primaryTextColor - dark purple-tinted
        hexToColor("#443848ff"),      // secondaryTextColor - dark purple-tinted
        hexToColor("#F8F5FCff"),      // notMutedColor - very light purple
        hexToColor("#E2D6F0ff"),      // middleColor - light purple-gray
        hexToColor("#D8CCE8ff"),      // altButtonColor - light purple-gray
        hexToColor("#FFFFFFff"),      // white
        hexToColor("#221E28ff"),      // black - purple-tinted black
        hexToColor("#241828ff"),      // sliderKnobColor - dark purple-tinted
        hexToColor("#F8F5FCff"),      // sliderBarColor - very light purple
        hexToColor("#8B6BA8ff"),      // clipColor - soft purple
        hexToColor("#EDE3F6ff"),      // lineColor - medium purple-gray
        hexToColor("#241828ff"),      // waveformColor - dark purple-tinted
        hexToColor("#8B6BA8ff"),      // selectedTrackColor - soft purple
        hexToColor("#D5C4E8ff"),      // automationLaneColor - light purple
        hexToColor("#8B6BA8ff")       // automationLabelColor - light purple-gray
    );
}

// Helper function to apply the theme
inline void applyTheme(UIResources& resources, const std::string& themeName) {
    auto& registry = getThemeRegistry();
    auto it = registry.find(themeName);
    if (it != registry.end()) {
        resources.activeTheme = const_cast<UITheme*>(it->second);
    } else {
        // Default to Dark theme
        resources.activeTheme = const_cast<UITheme*>(&Themes::Dark);
    }
}

inline std::string getAlignmentString(uilo::Align alignment) {
    int alignValue = static_cast<int>(alignment);

    constexpr int top = static_cast<int>(uilo::Align::TOP);
    constexpr int bottom = static_cast<int>(uilo::Align::BOTTOM);
    constexpr int left = static_cast<int>(uilo::Align::LEFT);
    constexpr int right = static_cast<int>(uilo::Align::RIGHT);
    constexpr int centerX = static_cast<int>(uilo::Align::CENTER_X);
    constexpr int centerY = static_cast<int>(uilo::Align::CENTER_Y);

    constexpr int topLeft = left | top;
    constexpr int topRight = right | top;
    constexpr int bottomLeft = left | bottom;
    constexpr int bottomRight = right | bottom;
    constexpr int topCenterX = top | centerX;
    constexpr int bottomCenterX = bottom | centerX;
    constexpr int leftCenterY = left | centerY;
    constexpr int rightCenterY = right | centerY;

    switch (alignValue) {
        case top:                   return "Top";
        case bottom:                return "Bottom";
        case left:                  return "Left";
        case right:                 return "Right";
        case centerX:               return "Center X";
        case centerY:               return "Center Y";
        case topLeft:               return "Top Left";
        case topRight:              return "Top Right";
        case bottomLeft:            return "Bottom Left";
        case bottomRight:           return "Bottom Right";
        case topCenterX:            return "Top Center X";
        case bottomCenterX:         return "Bottom Center X";
        case leftCenterY:           return "Left Center Y";
        case rightCenterY:          return "Right Center Y";
        default:                    return "Unknown";
    }
}